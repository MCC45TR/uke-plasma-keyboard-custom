// SPDX-License-Identifier: GPL-2.0-or-later
// Uses KWin's public-to-plugins InputDevice and Plugin interfaces. The virtual
// device pattern follows KDE's buttonrebinds plugin (David Redondo, GPL).
#include "protocol.h"
#include "refreshguard.h"
#include "versionguard.h"
#include <core/inputdevice.h>
#include <core/output.h>
#include <core/session.h>
#include <input.h>
#include <input_event.h>
#include <main.h>
#include <plugin.h>
#include <pointer_input.h>
#include <wayland_server.h>
#include <wayland/seat.h>
#include <window.h>
#include <workspace.h>
#include <KConfigGroup>
#include <KSharedConfig>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusServiceWatcher>
#include <QFileInfo>
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QVariantMap>
#include <QCoreApplication>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <functional>
#include <utility>
#include <linux/input-event-codes.h>
#include <unistd.h>

namespace {
using namespace KWin;
#ifdef UKE_KWIN_68
using OutputType = LogicalOutput;
using WindowRect = RectF;
#else
using OutputType = Output;
using WindowRect = QRectF;
#endif
auto now() { return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()); }
QRectF rectangle(const auto &r) { return {qreal(r.x()), qreal(r.y()), qreal(r.width()), qreal(r.height())}; }

class VirtualDevice final : public InputDevice {
public:
    std::function<void()> locksChanged;
    LEDs leds() const override { return m_leds; }
    void setLeds(LEDs leds) override {
        if (m_leds == leds) return;
        m_leds = leds;
        if (locksChanged) locksChanged();
    }
    QString name() const override { return QStringLiteral("Uke virtual keyboard and touchpad"); }
    bool isEnabled() const override { return true; }
    void setEnabled(bool) override { }
    bool isKeyboard() const override { return true; }
    bool isPointer() const override { return true; }
    bool isTouchpad() const override { return true; }
    bool isTouch() const override { return false; }
    bool isTabletTool() const override { return false; }
    bool isTabletPad() const override { return false; }
    bool isTabletModeSwitch() const override { return false; }
    bool isLidSwitch() const override { return false; }
private:
    LEDs m_leds;
};

class Backend final : public Plugin, protected QDBusContext, public InputEventFilter {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.uke.PlasmaKeyboard.Backend1")
public:
    Backend() : InputEventFilter(InputFilterOrder::LockScreen), m_bus(QDBusConnection::sessionBus()),
        m_watcher(this), m_refresh(this,[this](const QString &reason){Q_EMIT ProfileError(reason);})
    {
        if (!m_bus.registerService(QLatin1String(UkeKeyboard::service))) return;
        m_bus.registerObject(QLatin1String(UkeKeyboard::path), this,
                            QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals);
        m_watcher.setConnection(m_bus);
        m_watcher.setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
        connect(&m_watcher, &QDBusServiceWatcher::serviceUnregistered, this, [this] { detach(); });
        if(kwinApp()->session())connect(kwinApp()->session(),&Session::activeChanged,this,[this](bool active){if(!active){endStatic();release();}});
        m_device.locksChanged = [this] {
            if (m_client.isEmpty()) return;
            auto signal = QDBusMessage::createTargetedSignal(m_client, QLatin1String(UkeKeyboard::path),
                QLatin1String(UkeKeyboard::interface), QStringLiteral("Locks"));
            signal << m_device.leds().testFlag(LED::CapsLock) << m_device.leds().testFlag(LED::NumLock)
                   << m_device.leds().testFlag(LED::ScrollLock);
            m_bus.send(signal);
        };
        input()->addInputDevice(&m_device);
        m_deviceAdded = true;
        input()->installInputEventFilter(this);
        connect(workspace(), &Workspace::outputsChanged, this, [this] {
            if (m_active && !eligible(m_tabletName)) endStatic();
            m_refresh.topologyChanged();
            Q_EMIT TopologyChanged();
        });
        connect(workspace(), &Workspace::windowAdded, this, [this](Window *w) { observe(w); });
        connect(workspace(), &Workspace::windowRemoved, this, [this](Window *w) { m_saved.remove(w); });
        for (Window *w : workspace()->windows()) observe(w);
        m_bus.connect(QStringLiteral("org.freedesktop.ScreenSaver"), QStringLiteral("/ScreenSaver"),
                      QStringLiteral("org.freedesktop.ScreenSaver"), QStringLiteral("AboutToLock"), this, SLOT(Lock()));
        m_bus.connect(QStringLiteral("org.freedesktop.ScreenSaver"), QStringLiteral("/ScreenSaver"),
                      QStringLiteral("org.freedesktop.ScreenSaver"), QStringLiteral("ActiveChanged"), this, SLOT(Locked(bool)));
    }
    ~Backend() override {
        detach();
        if (m_deviceAdded) input()->removeInputDevice(&m_device);
        m_bus.unregisterObject(QLatin1String(UkeKeyboard::path));
        m_bus.unregisterService(QLatin1String(UkeKeyboard::service));
    }
    bool touchDown(TouchDownEvent *event) override {
        m_touches[event->id]=event->pos;relayTouch(0,event->id,event->pos);
        if(!m_active||locked()||!tablet()||!rectangle(tablet()->geometry()).contains(event->pos))return false;
        auto *window=input()->findToplevel(event->pos);
        if(!window||uint(window->pid())!=m_clientPid||!window->surface())return false;
        // Forward only the keyboard's own surface before the global touchscreen
        // gesture filter. Three/four-finger actions come from the virtual pad;
        // ordinary multi-finger typing must not also switch desktops.
        auto *seat=waylandServer()->seat();seat->setTimestamp(event->time);
        auto *point=seat->notifyTouchDown(window->surface(),window->bufferGeometry().topLeft(),event->id,event->pos);
        if(!point)return false;
        connect(window,&Window::bufferGeometryChanged,point,[window,point]{point->setSurfacePosition(window->bufferGeometry().topLeft());});
        m_directTouches.insert(event->id);m_directFrame=true;return true;
    }
    bool touchMotion(TouchMotionEvent *event) override {
        m_touches[event->id]=event->pos;relayTouch(1,event->id,event->pos);
        if(locked()){endStatic();release();return false;}
        if(m_cancelledTouches.contains(event->id))return true;
        if(!m_directTouches.contains(event->id))return false;
        auto *seat=waylandServer()->seat();seat->setTimestamp(event->time);seat->notifyTouchMotion(event->id,event->pos);m_directFrame=true;return true;
    }
    bool touchUp(TouchUpEvent *event) override {
        relayTouch(2,event->id,m_touches.take(event->id));
        if(locked()){endStatic();release();return false;}
        if(m_cancelledTouches.remove(event->id))return true;
        if(!m_directTouches.remove(event->id))return false;
        auto *seat=waylandServer()->seat();seat->setTimestamp(event->time);seat->notifyTouchUp(event->id);m_directFrame=true;return true;
    }
    bool touchFrame() override {if(!std::exchange(m_directFrame,false))return !m_cancelledTouches.isEmpty();waylandServer()->seat()->notifyTouchFrame();return true;}
    bool touchCancel() override {
        relayTouch(3,0,{});m_touches.clear();const bool consumed=!m_directTouches.isEmpty()||!m_cancelledTouches.isEmpty();
        if(!m_directTouches.isEmpty())waylandServer()->seat()->notifyTouchCancel();m_directTouches.clear();m_cancelledTouches.clear();m_directFrame=false;return consumed;
    }
    bool pointerMotion(PointerMotionEvent *event) override {
        if (!m_active || m_warping) return false;
        if (locked()) { endStatic(); return false; }
        const QPointF target = constrain(event->position);
        if (target != event->position) {
            event->position = target;
            m_warping = true;
            input()->pointer()->warp(target);
            m_warping = false;
        }
        return false;
    }

public Q_SLOTS:
    QVariantMap Capabilities() {
        QStringList internal, external;
        for (auto *o : workspace()->outputs()) {
            if (o->isPlaceholder()) continue;
            (o->isInternal() ? internal : external).append(o->name());
        }
        return {{QStringLiteral("version"), UkeKeyboard::protocolVersion},
                {QStringLiteral("internal"), internal}, {QStringLiteral("external"), external},
                {QStringLiteral("active"), m_active}, {QStringLiteral("tablet"), m_tabletName}};
    }
    bool Attach() {
        if (!calledFromDBus() || locked()) return false;
        const QString sender = message().service();
        const auto uid = m_bus.interface()->serviceUid(sender);
        const auto pid = m_bus.interface()->servicePid(sender);
        if (!uid.isValid() || uid.value() != getuid() || !pid.isValid()) return false;
        const QFileInfo executable(QStringLiteral("/proc/%1/exe").arg(pid.value()));
        const QString target = executable.symLinkTarget();
        const QFileInfo installed(QStringLiteral("/usr/bin/uke-plasma-keyboard-custom"));
        if (target != installed.canonicalFilePath() || installed.ownerId() != 0 ||
            (installed.permissions() & (QFile::WriteGroup | QFile::WriteOther))) return false;
        const QString selected = KSharedConfig::openConfig(QStringLiteral("kwinrc"))->group(QStringLiteral("Wayland"))
                                     .readEntry(QStringLiteral("InputMethod"), QString());
        if (!selected.endsWith(QLatin1String("/org.uke.plasma.keyboard.desktop")) &&
            selected != QLatin1String("org.uke.plasma.keyboard.desktop")) return false;
        if (!m_client.isEmpty() && m_client != sender) return false;
        m_client = sender;
        m_clientPid = pid.value();
        m_watcher.addWatchedService(sender);
        return true;
    }
    bool BeginStatic(const QString &tablet) {
        if (!authorized() || !eligible(tablet)) return false;
        if (m_active) return m_tabletName == tablet;
        m_tabletName = tablet;
        m_active = true;
        m_refresh.begin(tablet);
        for (Window *w : workspace()->windows()) moveAway(w);
        input()->pointer()->warp(constrain(input()->globalPointer()));
        Q_EMIT StaticChanged(true);
        return true;
    }
    bool Profile(const QString &profile) { return authorized() && m_refresh.set(profile); }
    QVariantMap Locks() {
        if (!authorized()) return {};
        return {{QStringLiteral("caps"),m_device.leds().testFlag(LED::CapsLock)},
                {QStringLiteral("num"),m_device.leds().testFlag(LED::NumLock)},
                {QStringLiteral("scroll"),m_device.leds().testFlag(LED::ScrollLock)}};
    }
    void EndStatic() { if (authorized(false)) endStatic(); }
    void ReleaseAll() { if (authorized(false)) release(); }
    void Key(uint code, bool pressed) {
        if (!authorized() || code == 0 || code > KEY_MICMUTE || code == KEY_POWER || code == KEY_SLEEP || code == KEY_SUSPEND || code == KEY_WAKEUP) return;
        if (pressed) { if (m_keys.contains(code)) return; m_keys.insert(code); }
        else { if (!m_keys.remove(code)) return; }
        Q_EMIT m_device.keyChanged(code, pressed ? KeyboardKeyState::Pressed : KeyboardKeyState::Released, now(), &m_device);
    }
    void Motion(double dx, double dy) {
        if (!authorized() || !std::isfinite(dx) || !std::isfinite(dy)) return;
        const QPointF delta(std::clamp(dx, -2048.0, 2048.0), std::clamp(dy, -2048.0, 2048.0));
        Q_EMIT m_device.pointerMotion(delta, delta, now(), &m_device);
        Q_EMIT m_device.pointerFrame(&m_device);
    }
    void Button(uint code, bool pressed) {
        if (!authorized() || code < BTN_LEFT || code > BTN_MIDDLE) return;
        if (pressed) { if (m_buttons.contains(code)) return; m_buttons.insert(code); }
        else { if (!m_buttons.remove(code)) return; }
        Q_EMIT m_device.pointerButtonChanged(code, pressed ? PointerButtonState::Pressed : PointerButtonState::Released, now(), &m_device);
        Q_EMIT m_device.pointerFrame(&m_device);
    }
    void Scroll(double x, double y) {
        if (!authorized() || !std::isfinite(x) || !std::isfinite(y)) return;
        axis(PointerAxis::Horizontal, x); axis(PointerAxis::Vertical, y);
        Q_EMIT m_device.pointerFrame(&m_device);
    }
    void Swipe(int fingers, int phase, double x, double y) {
        if (!authorized() || fingers < 3 || fingers > 4 || !std::isfinite(x) || !std::isfinite(y)) return;
        if (phase == 0 && !m_swiping) { m_swiping = true; Q_EMIT m_device.swipeGestureBegin(fingers, now(), &m_device); }
        else if (phase == 1 && m_swiping) Q_EMIT m_device.swipeGestureUpdate(QPointF(std::clamp(x,-2048.0,2048.0),std::clamp(y,-2048.0,2048.0)), now(), &m_device);
        else if (phase == 2 && m_swiping) { m_swiping = false; Q_EMIT m_device.swipeGestureEnd(now(), &m_device); }
        else if (phase == 3 && m_swiping) { m_swiping = false; Q_EMIT m_device.swipeGestureCancelled(now(), &m_device); }
    }
    void Pinch(int phase, double scale, double angle, double x, double y) {
        if (!authorized() || !std::isfinite(scale) || !std::isfinite(angle) || !std::isfinite(x) || !std::isfinite(y)) return;
        if (phase == 0 && !m_pinching) { m_pinching = true; Q_EMIT m_device.pinchGestureBegin(2, now(), &m_device); }
        else if (phase == 1 && m_pinching) Q_EMIT m_device.pinchGestureUpdate(std::clamp(scale,0.01,100.0),std::clamp(angle,-360.0,360.0),QPointF(x,y),now(),&m_device);
        else if (phase == 2 && m_pinching) { m_pinching=false; Q_EMIT m_device.pinchGestureEnd(now(),&m_device); }
        else if (phase == 3 && m_pinching) { m_pinching=false; Q_EMIT m_device.pinchGestureCancelled(now(),&m_device); }
    }
    void Lock() { endStatic(); release(); }
    void Locked(bool active) { if (active) Lock(); }

Q_SIGNALS:
    void StaticChanged(bool active);
    void TopologyChanged();
    void ProfileError(const QString &reason);

private:
    void relayTouch(int phase,int id,const QPointF &position) {
        if(m_client.isEmpty()||locked())return;
        OutputType *output=nullptr;
        for(auto *o:workspace()->outputs())if(rectangle(o->geometry()).contains(position)){output=o;break;}
        if(!output&&phase!=3)return;
        const QRectF area=output?rectangle(output->geometry()):QRectF(0,0,1,1);
        const QSize mm=output?output->physicalSize():QSize(200,140);
        auto signal=QDBusMessage::createTargetedSignal(m_client,QLatin1String(UkeKeyboard::path),QLatin1String(UkeKeyboard::interface),QStringLiteral("Touch"));
        signal<<phase<<id<<(position.x()-area.x())/std::max(1.0,area.width())<<(position.y()-area.y())/std::max(1.0,area.height())<<8.0/std::max(60,mm.width())<<8.0/std::max(60,mm.height());
        m_bus.send(signal);
    }
    struct Saved { QPointer<OutputType> output; QRectF before; QRectF moved; };
    bool locked() const { return !waylandServer() || waylandServer()->isScreenLocked() || !kwinApp()->session() || !kwinApp()->session()->isActive(); }
    bool authorized(bool requireUnlocked = true) {
        const bool own = calledFromDBus() && !m_client.isEmpty() && message().service() == m_client;
        if (own && requireUnlocked && locked()) { endStatic(); release(); return false; }
        return own;
    }
    OutputType *tablet() const {
        for (auto *o : workspace()->outputs()) if (o->name() == m_tabletName && o->isInternal()) return o;
        return nullptr;
    }
    QList<OutputType *> external() const {
        QList<OutputType *> result;
        for (auto *o : workspace()->outputs()) if (!o->isInternal() && !o->isPlaceholder()) result.append(o);
        return result;
    }
    bool eligible(const QString &name) const {
        OutputType *panel = nullptr;
        for (auto *o : workspace()->outputs()) if (o->name() == name && o->isInternal()) panel = o;
        if (!panel || locked()) return false;
        const QRectF area = rectangle(panel->geometry());
        const auto screens = external();
        return !screens.isEmpty() && std::none_of(screens.begin(), screens.end(), [&area](auto *o) { return rectangle(o->geometry()).intersects(area); });
    }
    QPointF constrain(QPointF p) const {
        const auto screens = external();
        QPointF best = p;
        double distance = std::numeric_limits<double>::max();
        for (auto *o : screens) {
            const QRectF r = rectangle(o->geometry());
            const QPointF q(std::clamp(p.x(),r.left(),r.right()-0.01),std::clamp(p.y(),r.top(),r.bottom()-0.01));
            const double d = (q.x()-p.x())*(q.x()-p.x())+(q.y()-p.y())*(q.y()-p.y());
            if (d < distance) { distance=d; best=q; }
        }
        return best;
    }
    void observe(Window *w) {
        connect(w, &Window::frameGeometryChanged, this, [this,w] { moveAway(w); });
        connect(w, &Window::outputChanged, this, [this,w] { moveAway(w); });
        moveAway(w);
    }
    void moveAway(Window *w) {
        if (!m_active || m_moving || !tablet() || !w || w->isSpecialWindow() || uint(w->pid()) == m_clientPid) return;
        const QRectF old = rectangle(w->frameGeometry());
        if (!old.intersects(rectangle(tablet()->geometry()))) return;
        const auto screens = external();
        if (screens.isEmpty()) return;
        auto *target = screens.first();
        for (auto *o : screens) if (rectangle(o->geometry()).contains(old.center())) { target=o; break; }
        if (!m_saved.contains(w)) m_saved.insert(w, {w->output(), old, {}});
        m_moving = true;
        w->sendToOutput(target);
        QRectF moved = rectangle(w->frameGeometry());
        const QRectF area = rectangle(target->geometry());
        moved.setSize(QSizeF(std::min(moved.width(),area.width()),std::min(moved.height(),area.height())));
        moved.moveLeft(std::clamp(moved.x(),area.left(),area.right()-moved.width()));
        moved.moveTop(std::clamp(moved.y(),area.top(),area.bottom()-moved.height()));
        w->moveResize(WindowRect(moved));
        m_saved[w].moved = rectangle(w->frameGeometry());
        m_moving = false;
    }
    void endStatic() {
        if (!m_active) return;
        m_active = false;
        m_refresh.end();
        release();
        const auto outputs = workspace()->outputs();
        for (auto it=m_saved.begin();it!=m_saved.end();++it) {
            Window *w=it.key(); const Saved &s=it.value();
            if (s.output && outputs.contains(s.output.data()) && rectangle(w->frameGeometry())==s.moved) {
                w->sendToOutput(s.output); w->moveResize(WindowRect(s.before));
            }
        }
        m_saved.clear(); m_tabletName.clear();
        Q_EMIT StaticChanged(false);
    }
    void release() {
        if(!m_directTouches.isEmpty()){
            waylandServer()->seat()->notifyTouchCancel();m_cancelledTouches.unite(std::exchange(m_directTouches,{}));m_directFrame=false;
        }
        const auto keys=std::exchange(m_keys,{});
        const auto buttons=std::exchange(m_buttons,{});
        for(uint k:keys) Q_EMIT m_device.keyChanged(k,KeyboardKeyState::Released,now(),&m_device);
        for(uint b:buttons) Q_EMIT m_device.pointerButtonChanged(b,PointerButtonState::Released,now(),&m_device);
        if(m_swiping) Q_EMIT m_device.swipeGestureCancelled(now(),&m_device);
        if(m_pinching) Q_EMIT m_device.pinchGestureCancelled(now(),&m_device);
        m_swiping=false; m_pinching=false;
        axis(PointerAxis::Horizontal,0); axis(PointerAxis::Vertical,0);
        Q_EMIT m_device.pointerFrame(&m_device);
    }
    void detach() {
        endStatic(); release();
        if(!m_client.isEmpty())m_watcher.removeWatchedService(m_client);
        m_client.clear(); m_clientPid=0;
    }
    void axis(PointerAxis orientation,double value) {
#ifdef UKE_KWIN_68
        Q_EMIT m_device.pointerAxisChanged(orientation,std::clamp(value,-4096.0,4096.0),0,PointerAxisSource::Finger,false,false,now(),&m_device);
#else
        Q_EMIT m_device.pointerAxisChanged(orientation,std::clamp(value,-4096.0,4096.0),0,PointerAxisSource::Finger,false,now(),&m_device);
#endif
    }
    QDBusConnection m_bus;
    QDBusServiceWatcher m_watcher;
    VirtualDevice m_device;
    RefreshGuard m_refresh;
    QString m_client,m_tabletName;
    uint m_clientPid=0;
    bool m_active=false,m_warping=false,m_moving=false,m_deviceAdded=false,m_swiping=false,m_pinching=false;
    QSet<uint> m_keys,m_buttons;
    QHash<int,QPointF>m_touches;
    QSet<int>m_directTouches,m_cancelledTouches;
    bool m_directFrame=false;
    QHash<Window *,Saved> m_saved;
};
}

class KWIN_EXPORT UkeKeyboardFactory final : public KWin::PluginFactory {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID PluginFactory_iid FILE "metadata.json")
    Q_INTERFACES(KWin::PluginFactory)
public:
    std::unique_ptr<KWin::Plugin> create() const override {
        if (!UkeKeyboard::compatibleKWin(QCoreApplication::applicationVersion(), QStringLiteral(UKE_KWIN_BUILD_VERSION))) {
            qWarning("Uke keyboard: rebuild the integration for the running KWin version");
            return {};
        }
        return std::make_unique<Backend>();
    }
};
#include "backend.moc"
