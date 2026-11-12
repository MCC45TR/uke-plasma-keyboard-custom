// SPDX-FileCopyrightText: 2026 Aleix Pol <aleixpol@kde.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <KConfig>
#include <KConfigGroup>
#include <QCoreApplication>
#include <QDir>
#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QPointer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QWindow>
#include <QtTest/QTest>

#include <memory>

#include <QtWaylandCompositor/QWaylandCompositor>
#include <QtWaylandCompositor/QWaylandCompositorExtension>
#include <QtWaylandCompositor/QWaylandCompositorExtensionTemplate>
#include <QtWaylandCompositor/QWaylandOutput>
#include <QtWaylandCompositor/QWaylandOutputMode>
#include <QtWaylandCompositor/QWaylandSeat>
#include <QtWaylandCompositor/QWaylandSurface>
#include <QtWaylandCompositor/QWaylandXdgShell>
#include <private/qxkbcommon_p.h>

#include "mockinputmethodcompositor_config.h"
#include "qwayland-server-input-method-unstable-v1.h"
#include "qwayland-server-wayland.h"
#include "qwayland-server-wlr-layer-shell-unstable-v1.h"

#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon-compose.h>

#define PLASMA_KEYBOARD_UNDER_GDB 0

using namespace Qt::StringLiterals;

/**
 * Whether we are running in a CI environment or not.
 *
 * This is used to adjust timing and wait durations in tests, as CI environments can have unpredictable timing.
 */
static const bool RUNNING_IN_CI = qEnvironmentVariableIsSet("CI");

static int createAnonymousKeymapFile(off_t size)
{
    int fd = -1;
#ifdef MFD_CLOEXEC
    fd = memfd_create("plasma-keyboard-keymap", MFD_CLOEXEC);
    if (fd >= 0) {
        if (ftruncate(fd, size) != 0) {
            close(fd);
            return -1;
        }
        return fd;
    }
#endif
    char name[] = "/tmp/plasma-keyboard-keymap-XXXXXX";
    fd = mkstemp(name);
    if (fd < 0) {
        return -1;
    }
    unlink(name);
    if (ftruncate(fd, size) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

class InputMethodKeyboard : public QObject, public QtWaylandServer::wl_keyboard
{
    Q_OBJECT
public:
    InputMethodKeyboard(wl_client *client, uint32_t id, int version)
        : QtWaylandServer::wl_keyboard(client, id, version)
    {
        initializeKeymap();
        m_timer.start();
    }

    void setFocusSurface(wl_resource *surface)
    {
        if (!surface || m_focusSurface == surface) {
            return;
        }
        m_focusSurface = surface;
        send_enter(++m_serial, m_focusSurface, QByteArray());
    }

    void sendKey(uint32_t key, uint32_t state)
    {
        send_key(++m_serial, static_cast<uint32_t>(m_timer.elapsed()), key, state);
    }

    bool keymapped() const
    {
        return m_keymapped;
    }
    void sendModifierState(uint32_t depressed,uint32_t latched,uint32_t locked,uint32_t group)
    {
        send_modifiers(++m_serial,depressed,latched,locked,group);
    }

    /**
     * Sets an updated XKB keymap built from the given layout and variant.
     * Use this to simulate a compositor keymap change (e.g. switching to us/intl
     * so that dead keys produce the correct keysyms in the child process).
     *
     * @param layout XKB layout name (e.g. "us")
     * @param variant XKB variant name (e.g. "intl"), or nullptr for none
     */
    void setKeymap(const char *layout, const char *variant = nullptr)
    {
        QXkbCommon::ScopedXKBContext context(xkb_context_new(XKB_CONTEXT_NO_FLAGS));
        if (!context) {
            qWarning() << "Failed to create xkb context";
            return;
        }

        xkb_rule_names names = {};
        names.rules = "evdev";
        names.layout = layout;
        names.variant = variant ? variant : "";
        names.options = "compose:menu";

        QXkbCommon::ScopedXKBKeymap keymap(xkb_keymap_new_from_names(context.get(), &names, XKB_KEYMAP_COMPILE_NO_FLAGS));
        if (!keymap) {
            qWarning() << "Failed to create xkb keymap for layout" << layout << variant;
            return;
        }

        char *mapStr = xkb_keymap_get_as_string(keymap.get(), XKB_KEYMAP_FORMAT_TEXT_V1);
        if (!mapStr) {
            qWarning() << "Failed to get xkb keymap string";
            return;
        }

        const QByteArray mapData(mapStr);
        free(mapStr);

        const int fd = createAnonymousKeymapFile(mapData.size() + 1);
        if (fd < 0) {
            qWarning() << "Failed to create keymap file";
            return;
        }

        if (write(fd, mapData.constData(), mapData.size()) != mapData.size()) {
            close(fd);
            qWarning() << "Failed to write keymap";
            return;
        }
        if (write(fd, "\0", 1) != 1) {
            close(fd);
            qWarning() << "Failed to write keymap terminator";
            return;
        }
        if (lseek(fd, 0, SEEK_SET) < 0) {
            close(fd);
            qWarning() << "Failed to seek keymap file";
            return;
        }

        send_keymap(WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, mapData.size() + 1);
        close(fd);
    }

Q_SIGNALS:
    void keymapDone();

private:
    void initializeKeymap()
    {
        setKeymap("us");
        m_keymapped = true;
        Q_EMIT keymapDone();
    }

    bool m_keymapped = false;
    uint32_t m_serial = 0;
    QElapsedTimer m_timer;
    wl_resource *m_focusSurface = nullptr;
};

class InputMethodContext : public QObject, public QtWaylandServer::zwp_input_method_context_v1
{
    Q_OBJECT

public:
    explicit InputMethodContext(wl_resource *focusSurface, QObject *parent = nullptr)
        : QObject(parent)
        , m_focusSurface(focusSurface)
    {
        m_xkbContext.reset(xkb_context_new(XKB_CONTEXT_NO_FLAGS));
        xkb_rule_names names = {};
        names.rules = "evdev";
        names.layout = "us";
        m_xkbKeymap.reset(xkb_keymap_new_from_names(m_xkbContext.get(), &names, XKB_KEYMAP_COMPILE_NO_FLAGS));
        m_xkbState.reset(xkb_state_new(m_xkbKeymap.get()));
    }

    InputMethodKeyboard *keyboard() const
    {
        return m_keyboard.get();
    }

Q_SIGNALS:
    void keyboardGrabbed();
    void commitStringChanged(const QString &commitString);
    void keysymReceived(uint32_t sym, uint32_t state);
    void keyReceived(uint32_t key, uint32_t state);
    void modifiersReceived(uint32_t depressed,uint32_t latched,uint32_t locked,uint32_t group);

protected:
    void zwp_input_method_context_v1_destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void zwp_input_method_context_v1_commit_string(Resource *resource, uint32_t serial, const QString &text) override
    {
        Q_UNUSED(resource);
        qInfo().noquote() << "commit_string" << serial << text;
        Q_EMIT commitStringChanged(text);
    }

    void zwp_input_method_context_v1_preedit_string(Resource *resource, uint32_t serial, const QString &text, const QString &commit) override
    {
        Q_UNUSED(resource);
        qInfo().noquote() << "preedit_string" << serial << text << commit;
    }

    void zwp_input_method_context_v1_delete_surrounding_text(Resource *resource, int32_t index, uint32_t length) override
    {
        Q_UNUSED(resource);
        qInfo() << "delete_surrounding_text" << index << length;
    }

    void zwp_input_method_context_v1_keysym(Resource *resource, uint32_t serial, uint32_t time, uint32_t sym, uint32_t state, uint32_t modifiers) override
    {
        Q_UNUSED(resource);
        qInfo() << "keysym" << serial << time << sym << state << modifiers;
        Q_EMIT keysymReceived(sym, state);
    }

    void zwp_input_method_context_v1_grab_keyboard(Resource *resource, uint32_t keyboard) override
    {
        m_keyboard = std::make_unique<InputMethodKeyboard>(resource->client(), keyboard, resource->version());
        m_keyboard->setFocusSurface(m_focusSurface);
        Q_EMIT keyboardGrabbed();
        qInfo() << "input_method_context grab_keyboard";
    }

    void zwp_input_method_context_v1_key(Resource *resource, uint32_t serial, uint32_t time, uint32_t key, uint32_t state) override
    {
        Q_UNUSED(resource);
        Q_UNUSED(serial);
        Q_UNUSED(time);
        Q_EMIT keyReceived(key, state);
        // Treat both PRESSED and REPEATED as press events (real compositors process
        // auto-repeat keys the same way, using REPEATED to inform clients that this is an
        // auto-repeat event).
        if (state != WL_KEYBOARD_KEY_STATE_PRESSED && state != WL_KEYBOARD_KEY_STATE_REPEATED) {
            return;
        }
        const uint32_t code = key + 8; // XKB keycode = Linux scancode + 8
        char buf[8] = {};
        xkb_state_key_get_utf8(m_xkbState.get(), code, buf, sizeof(buf));
        const QString text = QString::fromUtf8(buf);
        if (!text.isEmpty()) {
            qInfo().noquote() << "raw key commit_string" << text;
            Q_EMIT commitStringChanged(text);
        }
    }

    void zwp_input_method_context_v1_modifiers(Resource *resource,
                                               uint32_t serial,
                                               uint32_t mods_depressed,
                                               uint32_t mods_latched,
                                               uint32_t mods_locked,
                                               uint32_t group) override
    {
        Q_UNUSED(resource);
        Q_UNUSED(serial);
        xkb_state_update_mask(m_xkbState.get(), mods_depressed, mods_latched, mods_locked, 0, 0, group);
        Q_EMIT modifiersReceived(mods_depressed,mods_latched,mods_locked,group);
    }

private:
    std::unique_ptr<InputMethodKeyboard> m_keyboard;
    wl_resource *m_focusSurface = nullptr;
    QXkbCommon::ScopedXKBContext m_xkbContext;
    QXkbCommon::ScopedXKBKeymap m_xkbKeymap;
    QXkbCommon::ScopedXKBState m_xkbState;
};

class InputPanelSurface : public QObject, public QtWaylandServer::zwp_input_panel_surface_v1
{
    Q_OBJECT

public:
    InputPanelSurface(QWaylandSurface *surface, wl_client *client, uint32_t id, int version, QObject *parent = nullptr)
        : QObject(parent)
        , QtWaylandServer::zwp_input_panel_surface_v1(client, id, version)
        , m_surface(surface)
    {
    }

Q_SIGNALS:
    void toplevelRequested();
    void overlayRequested();

protected:
    void zwp_input_panel_surface_v1_set_toplevel(Resource *resource, wl_resource *output, uint32_t position) override
    {
        Q_UNUSED(resource);
        Q_UNUSED(output);
        Q_UNUSED(position);
        Q_EMIT toplevelRequested();
        qInfo() << "input_panel_surface set_toplevel";
    }

    void zwp_input_panel_surface_v1_set_overlay_panel(Resource *resource) override
    {
        Q_UNUSED(resource);
        Q_EMIT overlayRequested();
        qInfo() << "input_panel_surface set_overlay_panel";
    }

    void zwp_input_panel_surface_v1_destroy_resource(Resource *resource) override
    {
        Q_UNUSED(resource);
        delete this;
    }

private:
    QWaylandSurface *m_surface = nullptr;
};

class InputPanelV1 : public QWaylandCompositorExtensionTemplate<InputPanelV1>, public QtWaylandServer::zwp_input_panel_v1
{
    Q_OBJECT

public:
    explicit InputPanelV1(QWaylandCompositor *compositor)
        : QWaylandCompositorExtensionTemplate<InputPanelV1>(compositor)
    {
    }

    void initialize() override
    {
        QWaylandCompositorExtensionTemplate::initialize();
        auto *compositor = static_cast<QWaylandCompositor *>(extensionContainer());
        if (!compositor) {
            qWarning() << "No compositor available when initializing input panel";
            return;
        }
        init(compositor->display(), interfaceVersion());
    }

    int overlayPanelCount() const
    {
        return m_overlayPanelCount;
    }

    int toplevelPanelCount() const
    {
        return m_toplevelPanelCount;
    }

    wl_resource *surfaceResource() const
    {
        return m_surfaceResource;
    }

    QWaylandSurface *surface() const
    {
        if(m_overlaySelected&&m_surface&&m_surface->hasContent())return m_surface;
        if(m_keyboardSurface)return m_keyboardSurface;
        return m_surface;
    }
    void setKeyboardSurface(QWaylandSurface *surface){m_keyboardSurface=surface;}

Q_SIGNALS:
    void inputPanelSurfaceCreated();
    void overlayPanelRequested();
    void toplevelPanelRequested();

protected:
    void zwp_input_panel_v1_get_input_panel_surface(Resource *resource, uint32_t id, wl_resource *surface) override
    {
        auto *wlSurface = QWaylandSurface::fromResource(surface);
        if (!wlSurface) {
            qWarning() << "input_panel_surface requested for unknown surface";
            return;
        }

        m_surfaceResource = surface;
        m_surface = wlSurface;
        connect(wlSurface, &QWaylandSurface::surfaceDestroyed, this, [this, wlSurface] {
            if (m_surface == wlSurface) {
                m_surface = nullptr;
                m_surfaceResource = nullptr;
            }
        });
        connect(wlSurface, &QWaylandSurface::damaged, this, [this, wlSurface] {
            wlSurface->frameStarted();
            wlSurface->sendFrameCallbacks();
        });

        auto *panelSurface = new InputPanelSurface(wlSurface, resource->client(), id, resource->version(), this);
        connect(panelSurface, &InputPanelSurface::toplevelRequested, this, [this] {
            m_overlaySelected=false;
            ++m_toplevelPanelCount;
            Q_EMIT toplevelPanelRequested();
        });
        connect(panelSurface, &InputPanelSurface::overlayRequested, this, [this] {
            m_overlaySelected=true;
            ++m_overlayPanelCount;
            Q_EMIT overlayPanelRequested();
        });
        Q_EMIT inputPanelSurfaceCreated();
        qInfo() << "input_panel_surface created";
    }

private:
    int m_overlayPanelCount = 0;
    int m_toplevelPanelCount = 0;
    wl_resource *m_surfaceResource = nullptr;
    QWaylandSurface *m_surface = nullptr;
    QPointer<QWaylandSurface> m_keyboardSurface;
    bool m_overlaySelected=false;
};

// Minimal server for the real visible layer surface; input-panel-v1 above is
// retained for the compositor's invisible geometry stub and alternate overlay.
class TestLayerSurface final:public QObject,public QtWaylandServer::zwlr_layer_surface_v1
{
public:
    TestLayerSurface(QWaylandSurface *surface,wl_client *client,uint32_t id,int version,QObject *parent)
        :QObject(parent),QtWaylandServer::zwlr_layer_surface_v1(client,id,version),m_surface(surface)
    {
        connect(surface,&QWaylandSurface::damaged,this,[surface]{surface->frameStarted();surface->sendFrameCallbacks();});
        QTimer::singleShot(0,this,[this]{send_configure(++m_serial,m_width,m_height);});
    }
protected:
    void zwlr_layer_surface_v1_set_size(Resource*,uint32_t width,uint32_t height)override{m_width=width?width:1280;m_height=height?height:720;}
    void zwlr_layer_surface_v1_set_anchor(Resource*,uint32_t)override{}
    void zwlr_layer_surface_v1_set_exclusive_zone(Resource*,int32_t)override{}
    void zwlr_layer_surface_v1_set_margin(Resource*,int32_t,int32_t,int32_t,int32_t)override{}
    void zwlr_layer_surface_v1_set_keyboard_interactivity(Resource*,uint32_t)override{}
    void zwlr_layer_surface_v1_get_popup(Resource*,wl_resource*)override{}
    void zwlr_layer_surface_v1_ack_configure(Resource*,uint32_t)override{}
    void zwlr_layer_surface_v1_set_layer(Resource*,uint32_t)override{}
    void zwlr_layer_surface_v1_set_exclusive_edge(Resource*,uint32_t)override{}
    void zwlr_layer_surface_v1_destroy(Resource *resource)override{wl_resource_destroy(resource->handle);}
    void zwlr_layer_surface_v1_destroy_resource(Resource*)override{delete this;}
private:
    QPointer<QWaylandSurface>m_surface;
    uint32_t m_width=1280,m_height=720,m_serial=0;
};
class TestLayerShell final:public QWaylandCompositorExtensionTemplate<TestLayerShell>,public QtWaylandServer::zwlr_layer_shell_v1
{
    Q_OBJECT
public:
    explicit TestLayerShell(QWaylandCompositor *compositor):QWaylandCompositorExtensionTemplate<TestLayerShell>(compositor){}
    void initialize()override{
        QWaylandCompositorExtensionTemplate::initialize();
        init(static_cast<QWaylandCompositor*>(extensionContainer())->display(),interfaceVersion());
    }
Q_SIGNALS:
    void keyboardSurfaceCreated(QWaylandSurface *surface);
protected:
    void zwlr_layer_shell_v1_get_layer_surface(Resource *resource,uint32_t id,wl_resource *surface,wl_resource*,uint32_t,const QString&)override{
        auto *waylandSurface=QWaylandSurface::fromResource(surface);Q_ASSERT(waylandSurface);
        new TestLayerSurface(waylandSurface,resource->client(),id,resource->version(),this);
        Q_EMIT keyboardSurfaceCreated(waylandSurface);
    }
    void zwlr_layer_shell_v1_destroy(Resource *resource)override{wl_resource_destroy(resource->handle);}
};
class TestScreenSaver final:public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface","org.freedesktop.ScreenSaver")
public Q_SLOTS:
    bool GetActive()const{return false;}
};

class TestTouchActivation final : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.uke.PlasmaKeyboard.Backend1")
public:
    bool allowed = true;
    bool defer = false;
    int queries = 0;
    QDBusMessage pending;
public Q_SLOTS:
    bool TouchActivationAllowed() {
        ++queries;
        if (defer) { setDelayedReply(true); pending = message(); }
        return allowed;
    }
};

class TestVirtualKeyboard final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kwin.VirtualKeyboard")
    Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)
    Q_PROPERTY(int mode READ mode WRITE setMode)
public:
    bool visible() const { return m_visible; }
    int mode() const { return m_mode; }
    void setMode(int mode) { m_mode = mode; }
    void setVisible(bool visible) {
        if (m_visible == visible) return;
        m_visible = visible;
        Q_EMIT visibleChanged();
    }
Q_SIGNALS:
    void visibleChanged();
private:
    bool m_visible = false;
    int m_mode = 1;
};

class InputMethodV1 : public QWaylandCompositorExtensionTemplate<InputMethodV1>, public QtWaylandServer::zwp_input_method_v1
{
    Q_OBJECT
public:
    explicit InputMethodV1(QWaylandCompositor *compositor)
        : QWaylandCompositorExtensionTemplate<InputMethodV1>(compositor)
    {
    }

    void setInputPanel(InputPanelV1 *inputPanel)
    {
        m_inputPanel = inputPanel;
    }

    void initialize() override
    {
        QWaylandCompositorExtensionTemplate::initialize();
        auto *compositor = static_cast<QWaylandCompositor *>(extensionContainer());
        if (!compositor) {
            qWarning() << "No compositor available when initializing input method";
            return;
        }
        init(compositor->display(), interfaceVersion());
    }

    void sendActivate()
    {
        if (m_context) {
            return;
        }

        const wl_resource *focusSurface = m_inputPanel ? m_inputPanel->surfaceResource() : nullptr;
        m_context = std::make_unique<InputMethodContext>(const_cast<wl_resource *>(focusSurface), this);
        for (auto *resource : resourceMap()) {
            auto *contextResource = m_context->add(resource->client(), resource->version());
            send_activate(resource->handle, contextResource->handle);
            m_context->send_content_type(contextResource->handle,0,0);
            m_context->send_surrounding_text(contextResource->handle,QString(),0,0);
            m_context->send_commit_state(contextResource->handle,1);
        }
        qInfo() << "input_method_v1 activated";
        wl_display_flush_clients(static_cast<QWaylandCompositor *>(extensionContainer())->display());
        Q_EMIT activated();
    }

    void sendDeactivate()
    {
        if (!m_context) {
            return;
        }

        for (auto *resource : resourceMap()) {
            auto *contextResource = m_context->resourceMap().value(resource->client());
            if (contextResource) {
                send_deactivate(resource->handle, contextResource->handle);
            }
        }
        m_context.reset();
        wl_display_flush_clients(static_cast<QWaylandCompositor *>(extensionContainer())->display());
        qInfo() << "input_method_v1 deactivated";
    }

    InputMethodContext *context() const
    {
        return m_context.get();
    }

Q_SIGNALS:
    void activated();

protected:
    void zwp_input_method_v1_bind_resource(Resource *resource) override
    {
        if (m_context) {
            auto *contextResource = m_context->add(resource->client(), resource->version());
            send_activate(resource->handle, contextResource->handle);
            m_context->send_content_type(contextResource->handle,0,0);
            m_context->send_surrounding_text(contextResource->handle,QString(),0,0);
            m_context->send_commit_state(contextResource->handle,1);
            return;
        }

        if (!m_autoActivate || m_activatePending) {
            return;
        }

        m_activatePending = true;
        QTimer::singleShot(m_activateDelayMs, this, [this] {
            m_activatePending = false;
            sendActivate();
        });
    }

private:
    const bool m_autoActivate = true;
    const int m_activateDelayMs = 200;

    bool m_activatePending = false;
    std::unique_ptr<InputMethodContext> m_context;
    InputPanelV1 *m_inputPanel = nullptr;
};

class MockInputMethodCompositorTest : public QObject
{
    Q_OBJECT

public:
    MockInputMethodCompositorTest() = default;

private Q_SLOTS:
    void initTestCase()
    {
        if(qgetenv("PLASMA_KEYBOARD_TEST_ISOLATED_BUS")!="1")QSKIP("A private dbus-run-session is required");
        auto bus=QDBusConnection::sessionBus();
        QVERIFY(bus.registerService(QStringLiteral("org.freedesktop.ScreenSaver")));
        QVERIFY(bus.registerObject(QStringLiteral("/ScreenSaver"),&m_screenSaver,QDBusConnection::ExportAllSlots));
        QVERIFY(bus.registerService(QStringLiteral("org.uke.PlasmaKeyboard.Backend")));
        QVERIFY(bus.registerObject(QStringLiteral("/org/uke/PlasmaKeyboard/Backend"), &m_touchActivation, QDBusConnection::ExportAllSlots));
        QVERIFY(bus.registerService(QStringLiteral("org.kde.KWin")));
        QVERIFY(bus.registerObject(QStringLiteral("/VirtualKeyboard"), &m_virtualKeyboard,
            QDBusConnection::ExportAllProperties | QDBusConnection::ExportAllSignals));
        // Keep test configuration/data separate without changing HOME.
        if (!m_home.isValid()) {
            qFatal("Couldn't create temporary configuration folder for the test");
            return;
        }

        qputenv("XDG_CONFIG_HOME", qPrintable(m_home.path() + QStringLiteral("/.config")));
        qputenv("XDG_DATA_HOME", qPrintable(m_home.path() + QStringLiteral("/.local/share")));
        qputenv("XDG_CACHE_HOME", qPrintable(m_home.path() + QStringLiteral("/.cache")));
        qputenv("XDG_RUNTIME_DIR",m_runtimeDir.path().toUtf8());

        {
            KConfig cfg(QStringLiteral("uke-plasma-keyboard-customrc"));
            KConfigGroup grp(&cfg, QStringLiteral("General"));
            grp.writeEntry(QStringLiteral("enabledLocales"), QStringLiteral("it_IT"));
            grp.writeEntry(QStringLiteral("keyboardNavigationEnabled"), true);
            grp.writeEntry(QStringLiteral("defaultLocale"),QStringLiteral("it_IT"));
            KConfigGroup laptop(&cfg,QStringLiteral("Laptop"));
            laptop.writeEntry(QStringLiteral("syncLayout"),false);
            laptop.writeEntry(QStringLiteral("VirtualLayoutList"),QStringList{QStringLiteral("it")});
            laptop.writeEntry(QStringLiteral("VirtualVariantList"),QStringList{QString()});
            // Set the long press threshold to max to avoid potential flakiness in CI where timing can be unpredictable.
            grp.writeEntry(QStringLiteral("diacriticsHoldThresholdMs"), 1500);
        }

        m_compositor = std::make_unique<QWaylandCompositor>();
        m_socketPath = m_runtimeDir.path() + QLatin1String("/plasma-keyboard-mock-") + QString::number(QCoreApplication::applicationPid());
        m_compositor->setSocketName(m_socketPath.toUtf8());
        m_compositor->setUseHardwareIntegrationExtension(false);
        m_compositor->create();
        QTRY_VERIFY_WITH_TIMEOUT(m_compositor->isCreated(), 2000);

        m_seat = m_compositor->defaultSeat();
        QVERIFY(m_seat);

        m_outputWindow = std::make_unique<QWindow>();
        m_outputWindow->setGeometry(0, 0, 1280, 720);
        m_outputWindow->setTitle(u"Mock Compositor Output"_s);
        m_outputWindow->setVisible(false);

        m_output = std::make_unique<QWaylandOutput>(m_compositor.get(), m_outputWindow.get());
        m_output->setManufacturer(u"Mock"_s);
        m_output->setModel(u"InputMethod"_s);
        m_output->addMode(QWaylandOutputMode(QSize(1280, 720), 60000), true);
        m_output->setCurrentMode(m_output->modes().first());
        m_compositor->setDefaultOutput(m_output.get());

        m_xdgShell = std::make_unique<QWaylandXdgShell>(m_compositor.get());
        m_xdgShell->initialize();

        m_inputMethod = std::make_unique<InputMethodV1>(m_compositor.get());
        m_inputMethod->initialize();

        m_inputPanel = std::make_unique<InputPanelV1>(m_compositor.get());
        m_inputPanel->initialize();
        m_inputMethod->setInputPanel(m_inputPanel.get());
        m_layerShell=std::make_unique<TestLayerShell>(m_compositor.get());
        connect(m_layerShell.get(),&TestLayerShell::keyboardSurfaceCreated,m_inputPanel.get(),&InputPanelV1::setKeyboardSurface);
        m_layerShell->initialize();

        m_child = std::make_unique<QProcess>();
        auto *child = m_child.get();
        connect(child, &QProcess::readyReadStandardError, this, [child] {
            QTextStream(stderr) << child->readAllStandardError();
        });
        connect(child, &QProcess::readyReadStandardOutput, this, [child] {
            QTextStream(stdout) << child->readAllStandardOutput();
        });
        connect(child, &QProcess::finished, this, [child] {
            qWarning() << "child state:" << child->state() << "error:" << child->error() << child->errorString() << "exitStatus:" << child->exitStatus()
                       << "exitCode:" << child->exitCode();
        });
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert(u"WAYLAND_DISPLAY"_s, m_socketPath);
        env.insert(u"QT_QPA_PLATFORM"_s, u"wayland"_s);
        env.insert(u"XDG_RUNTIME_DIR"_s, m_runtimeDir.path());
        env.insert(u"QT_QUICK_BACKEND"_s, u"software"_s); // Without this plasma-keyboard explodes on alpine for some reason
        env.insert(u"QT_FORCE_STDERR_LOGGING"_s,u"1"_s);
        env.insert(u"UKE_KEYBOARD_TEST_KEY_FILE"_s,m_runtimeDir.path()+QStringLiteral("/q-key.json"));
        m_child->setProcessEnvironment(env);

#if PLASMA_KEYBOARD_UNDER_GDB
        m_child->setProgram(QStringLiteral("gdb"));
        m_child->setArguments({QStringLiteral("-batch"),
                               QStringLiteral("-ex"),
                               QStringLiteral("set pagination off"),
                               QStringLiteral("-ex"),
                               QStringLiteral("run"),
                               QStringLiteral("-ex"),
                               QStringLiteral("bt"),
                               QStringLiteral("-ex"),
                               QStringLiteral("quit"),
                               QStringLiteral("--args"),
                               QStringLiteral(PLASMA_KEYBOARD_BINARY_PATH)});
#else
        m_child->setProgram(QStringLiteral(PLASMA_KEYBOARD_BINARY_PATH));
#endif
        m_child->start();
        QVERIFY2(m_child->waitForStarted(), qPrintable(m_child->errorString()));

        qInfo().noquote().nospace() << "Compositor running on WAYLAND_DISPLAY=" << m_socketPath << " " << PLASMA_KEYBOARD_BINARY_PATH;
        {
            QSignalSpy spy(m_inputMethod.get(), &InputMethodV1::activated);
            QVERIFY(spy.count() || spy.wait());
        }

        QVERIFY(m_inputMethod->context());
        if (!m_inputMethod->context()->keyboard()) {
            QSignalSpy grabSpy(m_inputMethod->context(), &InputMethodContext::keyboardGrabbed);
            QVERIFY(grabSpy.wait());
        }

        auto *keyboard = m_inputMethod->context()->keyboard();
        QVERIFY(keyboard);
        if (!keyboard->keymapped()) {
            QSignalSpy grabSpy(keyboard, &InputMethodKeyboard::keymapDone);
            QVERIFY(grabSpy.wait());
        }
    }

    /**
     * Clean up after each test function.
     *
     * Drains any pending Wayland events from the preceding test so that
     * stale events/responses do not bleed into the next test.
     *
     * Uses longer waits in CI environments (where timing is less predictable)
     * and shorter waits locally to keep iteration fast.
     */
    void cleanup()
    {
        if(m_inputMethod&&m_inputMethod->context()&&m_inputMethod->context()->keyboard())m_inputMethod->context()->keyboard()->sendModifierState(0,0,0,0);
        QTest::qWait(RUNNING_IN_CI ? 200 : 20);
        wl_display_flush_clients(m_compositor->display());
        QTest::qWait(RUNNING_IN_CI ? 100 : 20);
    }

    void sendKey(int key, int interval)
    {
        auto keyboard = m_inputMethod->context()->keyboard();
        Q_ASSERT(keyboard);
        keyboard->sendKey(key, WL_KEYBOARD_KEY_STATE_PRESSED);
        wl_display_flush_clients(m_compositor->display());
        QTest::qWait(interval);
        keyboard->sendKey(key, WL_KEYBOARD_KEY_STATE_RELEASED);
        wl_display_flush_clients(m_compositor->display());
    }

    /**
     * Sets an updated keymap and waits for it to be processed.
     *
     * @param layout XKB layout name (e.g. "us")
     * @param variant XKB variant name (e.g. "intl"), or nullptr for none
     */
    void setKeymap(const char *layout, const char *variant = nullptr)
    {
        auto *keyboard = m_inputMethod->context()->keyboard();
        Q_ASSERT(keyboard);
        keyboard->setKeymap(layout, variant);
        wl_display_flush_clients(m_compositor->display());
        QTest::qWait(200);
    }

    /**
     * Helper that simulates a tap on the on-screen keyboard at the given position.
     *
     * Waits for the surface to have content before sending the tap, and waits briefly
     * after to allow the event to be processed.
     *
     * @param position The position to tap on the on-screen keyboard.
     */
    void tapInputPanel(const QPointF &position)
    {
        if (!m_inputPanel->surface()) {
            QSignalSpy surfaceSpy(m_inputPanel.get(), &InputPanelV1::inputPanelSurfaceCreated);
            QVERIFY(surfaceSpy.wait());
        }

        auto *surface = m_inputPanel->surface();
        QVERIFY(surface);
        QTRY_VERIFY_WITH_TIMEOUT(surface->hasContent(), 5000);

        constexpr int touchId = 0;
        m_seat->sendTouchPointPressed(surface, touchId, position);
        m_seat->sendTouchFrameEvent(surface->client());
        wl_display_flush_clients(m_compositor->display());

        QTest::qWait(50);

        m_seat->sendTouchPointReleased(surface, touchId, position);
        m_seat->sendTouchFrameEvent(surface->client());
        wl_display_flush_clients(m_compositor->display());
    }

    /**
     * Helper that returns the size of the input panel surface, or an empty size if not available.
     */
    QSize inputPanelSurfaceSize() const
    {
        auto *surface = m_inputPanel->surface();
        if (!surface) {
            return {};
        }

        const QSize destinationSize = surface->destinationSize();
        return destinationSize.isEmpty() ? surface->bufferSize() : destinationSize;
    }

    // Exercise the actual layer-shell application, not a visibility-policy copy.
    void testAutomaticOpeningRequiresTouchAndRejectsStaleReplies()
    {
        const auto visible = [this] { return m_inputPanel->surface() && m_inputPanel->surface()->hasContent(); };
        QTRY_VERIFY(visible());
        m_inputMethod->sendDeactivate();
        QTRY_VERIFY(!visible());
        m_touchActivation.allowed = false;
        const int before = m_touchActivation.queries;
        m_inputMethod->sendActivate();
        QTRY_VERIFY(m_touchActivation.queries > before);
        QTest::qWait(350);
        QVERIFY(!visible()); // Mouse/keyboard/programmatic activation.
        // A compositor visibility edge must not activate an unmapped layer
        // window and bypass the touch decision through QML's active handler.
        m_virtualKeyboard.setVisible(true);
        QTest::qWait(150);
        QVERIFY(!visible());
        m_virtualKeyboard.setVisible(false);
        QTest::qWait(100);
        auto *context = m_inputMethod->context();
        for (auto *resource : context->resourceMap()) {
            context->send_surrounding_text(resource->handle, QStringLiteral("synthetic"), 0, 0);
            context->send_commit_state(resource->handle, 2);
        }
        wl_display_flush_clients(m_compositor->display());
        QTest::qWait(150);
        QVERIFY(!visible());

        m_inputMethod->sendDeactivate();
        m_touchActivation.allowed = true;
        m_touchActivation.defer = true;
        const int delayed = m_touchActivation.queries;
        m_inputMethod->sendActivate();
        QTRY_VERIFY(m_touchActivation.queries > delayed);
        m_inputMethod->sendDeactivate();
        wl_display_flush_clients(m_compositor->display());
        QTest::qWait(100);
        QVERIFY(QDBusConnection::sessionBus().send(m_touchActivation.pending.createReply(QVariantList{true})));
        m_touchActivation.defer = false;
        QTest::qWait(150);
        QVERIFY(!visible()); // An obsolete touch reply cannot reopen a lost context.

        m_inputMethod->sendActivate();
        QTRY_VERIFY(visible()); // A current touch request opens normally.
    }

    /** Test that tapping an on-screen key commits the expected character. */
    void testOnScreenKeyboardCommitsCharacter()
    {
        if (!m_inputPanel->surface()) {
            QSignalSpy surfaceSpy(m_inputPanel.get(), &InputPanelV1::inputPanelSurfaceCreated);
            QVERIFY(surfaceSpy.wait());
        }

        auto *surface = m_inputPanel->surface();
        QVERIFY(surface);
        QTRY_VERIFY_WITH_TIMEOUT(surface->hasContent(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!inputPanelSurfaceSize().isEmpty(), 5000);

        const QString geometryPath=m_runtimeDir.path()+QStringLiteral("/q-key.json");
        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(geometryPath),5000);
        QTest::qWait(500); // allow font/style and the layer configure to settle
        QFile geometry(geometryPath);QVERIFY(geometry.open(QIODevice::ReadOnly));
        const auto point=QJsonDocument::fromJson(geometry.readAll()).object();
        const QPointF qKeyCenter(point.value(QStringLiteral("x")).toDouble(-1),point.value(QStringLiteral("y")).toDouble(-1));
        QVERIFY(qKeyCenter.x()>0&&qKeyCenter.y()>0);
        qInfo()<<"Synthetic Q geometry"<<qKeyCenter<<"surface"<<inputPanelSurfaceSize();
        QVERIFY(qKeyCenter.x()<inputPanelSurfaceSize().width()&&qKeyCenter.y()<inputPanelSurfaceSize().height());

        QSignalSpy commitStringSpy(m_inputMethod->context(), &InputMethodContext::commitStringChanged);
        tapInputPanel(qKeyCenter);
        QVERIFY(commitStringSpy.count() || commitStringSpy.wait());
        QCOMPARE(commitStringSpy.count(), 1);
        QCOMPARE(commitStringSpy.first().first().toString(), QStringLiteral("q"));
    }

    QPointF editingPosition(const QString &name)const
    {
        QFile file(m_runtimeDir.path()+QStringLiteral("/q-key.json"));
        if(!file.open(QIODevice::ReadOnly))return {-1,-1};
        const auto object=QJsonDocument::fromJson(file.readAll()).object();
        return {object.value(name+QStringLiteral("X")).toDouble(-1),object.value(name+QStringLiteral("Y")).toDouble(-1)};
    }
    void dragInputPanel(const QPointF &start,double distance)
    {
        auto *surface=m_inputPanel->surface();QVERIFY(surface);
        m_seat->sendTouchPointPressed(surface,0,start);m_seat->sendTouchFrameEvent(surface->client());wl_display_flush_clients(m_compositor->display());
        QTest::qWait(30);
        for(int step=1;step<=3;++step){m_seat->sendTouchPointMoved(surface,0,start+QPointF(distance*step/3,0));m_seat->sendTouchFrameEvent(surface->client());wl_display_flush_clients(m_compositor->display());QTest::qWait(30);}
        m_seat->sendTouchPointReleased(surface,0,start+QPointF(distance,0));m_seat->sendTouchFrameEvent(surface->client());wl_display_flush_clients(m_compositor->display());
        QTest::qWait(150);
    }
    void testSpaceDragMovesCursorWithoutSpace()
    {
        const auto point=editingPosition(QStringLiteral("space"));QVERIFY(point.x()>0&&point.y()>0);
        QSignalSpy commits(m_inputMethod->context(),&InputMethodContext::commitStringChanged);
        QSignalSpy keys(m_inputMethod->context(),&InputMethodContext::keysymReceived);
        dragInputPanel(point,30);
        QTRY_VERIFY(!keys.isEmpty());
        for(const auto &key:keys)QCOMPARE(key[0].toUInt(),uint(XKB_KEY_Right));
        QCOMPARE(keys.size()%2,0);QVERIFY(commits.isEmpty());
    }
    void testBackspaceDragDeletesWordAndRestoresModifiers()
    {
        const auto point=editingPosition(QStringLiteral("backspace"));QVERIFY(point.x()>0&&point.y()>0);
        // US fixture: Shift depressed, Caps locked. The temporary Ctrl must
        // preserve both and restore exactly this state after deletion.
        m_inputMethod->context()->keyboard()->sendModifierState(1,0,2,0);
        wl_display_flush_clients(m_compositor->display());QTest::qWait(100);
        QSignalSpy keys(m_inputMethod->context(),&InputMethodContext::keyReceived);
        QSignalSpy symbols(m_inputMethod->context(),&InputMethodContext::keysymReceived);
        QSignalSpy modifiers(m_inputMethod->context(),&InputMethodContext::modifiersReceived);
        dragInputPanel(point,-30);
        QTRY_VERIFY(keys.size()>=2);
        for(int i=0;i<keys.size();++i){QCOMPARE(keys[i][0].toUInt(),uint(KEY_BACKSPACE));QCOMPARE(keys[i][1].toUInt(),uint(i%2==0?WL_KEYBOARD_KEY_STATE_PRESSED:WL_KEYBOARD_KEY_STATE_RELEASED));}
        QCOMPARE(keys.size()%2,0);QVERIFY(symbols.isEmpty());
        QTRY_VERIFY(modifiers.size()>=2);
        QCOMPARE(modifiers.first()[0].toUInt(),5u);QCOMPARE(modifiers.last()[0].toUInt(),1u);
        QCOMPARE(modifiers.last()[1].toUInt(),0u);QCOMPARE(modifiers.last()[2].toUInt(),2u);QCOMPARE(modifiers.last()[3].toUInt(),0u);
    }

    void testKeyboardNavigationCommitsCharacter()
    {
        if (!m_inputPanel->surface()) {
            QSignalSpy surfaceSpy(m_inputPanel.get(), &InputPanelV1::inputPanelSurfaceCreated);
            QVERIFY(surfaceSpy.wait());
        }

        auto *surface = m_inputPanel->surface();
        QVERIFY(surface);
        QTRY_VERIFY_WITH_TIMEOUT(surface->hasContent(), 5000);

        QSignalSpy commitStringSpy(m_inputMethod->context(), &InputMethodContext::commitStringChanged);
        QSignalSpy keysymSpy(m_inputMethod->context(), &InputMethodContext::keysymReceived);

        // Qt chooses its initial highlight from the current geometry. Locate X
        // through real navigation instead of assuming three Right presses.
        auto selected=[this]{QFile file(m_runtimeDir.path()+QStringLiteral("/q-key.json"));if(!file.open(QIODevice::ReadOnly))return QString();return QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("navigation")).toString();};
        bool found=false;
        for(int row=0;row<5&&!found;++row){
            for(int column=0;column<16;++column){sendKey(KEY_RIGHT,100);QTest::qWait(110);if(selected()==QLatin1String("x")){found=true;break;}}
            if(!found)sendKey(KEY_DOWN,100);
        }
        QVERIFY2(found,"Navigation did not reach X in the configured layout");
        sendKey(KEY_ENTER, 100);

        QVERIFY(commitStringSpy.count() || commitStringSpy.wait());
        QCOMPARE(commitStringSpy.count(), 1);
        QCOMPARE(commitStringSpy.first().first().toString(), QStringLiteral("x"));

        for (const QList<QVariant> &args : keysymSpy) {
            const uint32_t sym = args.at(0).toUInt();
            QVERIFY2(sym != XKB_KEY_Right && sym != XKB_KEY_Return, "Keyboard navigation keys were incorrectly forwarded via keysym");
        }
    }

    void testLongPressShowsOverlayPanel()
    {
        QSignalSpy overlaySpy(m_inputPanel.get(), &InputPanelV1::overlayPanelRequested);

        sendKey(KEY_A, 2000);
        QVERIFY(overlaySpy.count() || overlaySpy.wait());

        QSignalSpy commitStringSpy(m_inputMethod->context(), &InputMethodContext::commitStringChanged);
        sendKey(KEY_1, 10);
        QVERIFY(commitStringSpy.count() || commitStringSpy.wait());
        QCOMPARE(commitStringSpy.count(), 1);
        QCOMPARE(commitStringSpy.first().first().toString(), QStringLiteral("à"));
    }

    /** Test that a short press of a key does not trigger the overlay panel and commits the expected character. */
    void testShortPressDoesNotShowOverlayPanel()
    {
        QSignalSpy overlaySpy(m_inputPanel.get(), &InputPanelV1::overlayPanelRequested);

        sendKey(KEY_A, 100);
        QTest::qWait(300); // give plasma-keyboard time to react
        QVERIFY(overlaySpy.isEmpty()); // now we're more confident

        QSignalSpy commitStringSpy(m_inputMethod->context(), &InputMethodContext::commitStringChanged);
        sendKey(KEY_1, 10);
        QVERIFY(commitStringSpy.count() || commitStringSpy.wait());
        QCOMPARE(commitStringSpy.count(), 1);
        QCOMPARE(commitStringSpy.first().first().toString(), QStringLiteral("1"));
    }

    /** Test that multiple short key presses (e.g. typing quickly) does not trigger the overlay panel. */
    void testMultipleShortPressesDoesNotShowOverlayPanel()
    {
        QSignalSpy overlaySpy(m_inputPanel.get(), &InputPanelV1::overlayPanelRequested);
        QSignalSpy commitStringSpy(m_inputMethod->context(), &InputMethodContext::commitStringChanged);

        const int interval = 50;

        // swim days
        sendKey(KEY_S, interval);
        sendKey(KEY_W, interval);
        sendKey(KEY_I, interval);
        sendKey(KEY_M, interval);
        sendKey(KEY_SPACE, interval);
        sendKey(KEY_D, interval);
        sendKey(KEY_A, interval);
        sendKey(KEY_Y, interval);
        sendKey(KEY_S, interval);

        QTest::qWait(300); // give plasma-keyboard time to react
        QVERIFY(overlaySpy.isEmpty()); // now we're more confident

        QTRY_COMPARE(commitStringSpy.count(), 9);
        QCOMPARE(commitStringSpy.at(0).first().toString(), QStringLiteral("s"));
        QCOMPARE(commitStringSpy.at(1).first().toString(), QStringLiteral("w"));
        QCOMPARE(commitStringSpy.at(2).first().toString(), QStringLiteral("i"));
        QCOMPARE(commitStringSpy.at(3).first().toString(), QStringLiteral("m"));
        QCOMPARE(commitStringSpy.at(4).first().toString(), QStringLiteral(" "));
        QCOMPARE(commitStringSpy.at(5).first().toString(), QStringLiteral("d"));
        QCOMPARE(commitStringSpy.at(6).first().toString(), QStringLiteral("a"));
        QCOMPARE(commitStringSpy.at(7).first().toString(), QStringLiteral("y"));
        QCOMPARE(commitStringSpy.at(8).first().toString(), QStringLiteral("s"));
    }

    /** Test that navigating the diacritics menu with arrow keys and pressing enter commits the correct character. */
    void testDiacriticsKeyNavigation()
    {
        QSignalSpy overlaySpy(m_inputPanel.get(), &InputPanelV1::overlayPanelRequested);

        sendKey(KEY_1, 2000);
        QVERIFY(overlaySpy.count() || overlaySpy.wait());

        QSignalSpy commitStringSpy(m_inputMethod->context(), &InputMethodContext::commitStringChanged);
        sendKey(KEY_RIGHT, 100);
        sendKey(KEY_RIGHT, 100);
        sendKey(KEY_ENTER, 100);
        QVERIFY(commitStringSpy.count() || commitStringSpy.wait());
        QCOMPARE(commitStringSpy.count(), 1);
        QCOMPARE(commitStringSpy.first().first().toString(), QStringLiteral("½"));
    }

    /**
     * Test that a Multi_key compose sequence (Multi_key + t + m → ™) is handled
     * locally by the XKB compose state machine in OverlayController. The
     * intermediate keys must be consumed and not forwarded to the compositor;
     * only the composed result should arrive via commit_string.
     */
    void testComposeSequence()
    {
        // Skip if no XKB compose table is available for the current locale.
        // Without one, OverlayController::m_xkbComposeState is null and the
        // compose sequence would not be processed locally.
        {
            QXkbCommon::ScopedXKBContext xkbContext(xkb_context_new(XKB_CONTEXT_NO_FLAGS));
            if (!xkbContext) {
                QSKIP("Could not create XKB context");
            }
            const char *locale = setlocale(LC_CTYPE, nullptr);
            if (!locale || locale[0] == '\0') {
                locale = "C";
            }
            xkb_compose_table *composeTable = xkb_compose_table_new_from_locale(xkbContext.get(), locale, XKB_COMPOSE_COMPILE_NO_FLAGS);
            if (!composeTable) {
                QSKIP("No XKB compose table available for the current locale");
            }
            xkb_compose_table_unref(composeTable);
        }

        QSignalSpy overlaySpy(m_inputPanel.get(), &InputPanelV1::overlayPanelRequested);
        QSignalSpy commitStringSpy(m_inputMethod->context(), &InputMethodContext::commitStringChanged);
        QSignalSpy keysymSpy(m_inputMethod->context(), &InputMethodContext::keysymReceived);

        // Send the compose sequence: Multi_key → t → m → ™
        // KEY_COMPOSE (127) maps to Multi_key via the compose:menu keymap option.
        sendKey(KEY_COMPOSE, 10);
        sendKey(KEY_T, 10);
        sendKey(KEY_M, 10);

        // Exactly one commit_string with the composed character should arrive.
        QTRY_COMPARE(commitStringSpy.count(), 1);
        QCOMPARE(commitStringSpy.first().first().toString(), QStringLiteral("™"));

        // The intermediate keys (t, m) must not have been forwarded via keysym.
        // XKB_KEY_t = 0x74, XKB_KEY_m = 0x6d
        for (const QList<QVariant> &args : keysymSpy) {
            const uint32_t sym = args.at(0).toUInt();
            QVERIFY2(sym != 0x74 && sym != 0x6d, "A compose sequence key (t or m) was incorrectly forwarded via keysym");
        }

        // No overlay should have appeared during the compose sequence.
        QTest::qWait(200);
        QVERIFY(overlaySpy.isEmpty());
    }

    /**
     * Test that pressing a dead key (e.g. the grave key `) followed by a character key (e.g. a) results
     * in the correct composed character (e.g. à) being committed, without the dead key
     * being forwarded as a separate keysym or commit_string.
     */
    void testDeadKeySequence()
    {
        // Switch to the US intl with dead keys layout for the test.
        setKeymap("us", "intl");

        QSignalSpy commitStringSpy(m_inputMethod->context(), &InputMethodContext::commitStringChanged);
        QSignalSpy keysymSpy(m_inputMethod->context(), &InputMethodContext::keysymReceived);

        // Send the dead key sequence: ` → a = à
        sendKey(KEY_GRAVE, 10);
        sendKey(KEY_A, 10);

        // Exactly one commit_string with the composed character should arrive.
        QTRY_COMPARE(commitStringSpy.count(), 1);
        QCOMPARE(commitStringSpy.first().first().toString(), QStringLiteral("à"));

        // The dead key (`) must not have been forwarded via keysym.
        // XKB_KEY_grave = 0x60
        for (const QList<QVariant> &args : keysymSpy) {
            const uint32_t sym = args.at(0).toUInt();
            QVERIFY2(sym != 0x60, "The dead key (`) was incorrectly forwarded via keysym");
        }

        // Set the keymap back to the default to avoid affecting other tests.
        setKeymap("us", nullptr);
    }

    /**
     * Verify holding down spacebar triggers auto-repeat, and results in multiple
     * commit_string signals with " ".
     */
    void testSpacebarRepeatWorks()
    {
        QSignalSpy commitStringSpy(m_inputMethod->context(), &InputMethodContext::commitStringChanged);

        // Press and hold spacebar to trigger auto-repeat
        sendKeyWithRepeat(KEY_SPACE);

        // We should have received multiple commit_string signals with " "
        QTRY_VERIFY(commitStringSpy.count() > 1);
        for (const QList<QVariant> &args : commitStringSpy) {
            QCOMPARE(args.first().toString(), QStringLiteral(" "));
        }
    }

    /**
     * Test that normal key press/releases of printable keys forwards the key events via
     * key() and not via commit_string. Non-diacritic keys should fall through to normal
     * forwarding in Keyboard::keyboard_key(), preserving compositor-side auto-repeat.
     */
    void testKeysForwardedViaKey()
    {
        QSignalSpy keySpy(m_inputMethod->context(), &InputMethodContext::keyReceived);

        // A key without diacritics options.
        sendKey(KEY_Z, 10);

        // The key should be forwarded via key() for both press and release.
        // Use the LAST two events to avoid capturing any stale events from a
        // previous test.
        QTRY_VERIFY(keySpy.count() >= 2);
        int last = keySpy.count() - 1;
        int secondLast = last - 1;

        QCOMPARE(keySpy.at(secondLast).at(0).toUInt(), static_cast<uint32_t>(KEY_Z));
        QCOMPARE(keySpy.at(secondLast).at(1).toUInt(), static_cast<uint32_t>(WL_KEYBOARD_KEY_STATE_PRESSED));
        QCOMPARE(keySpy.at(last).at(0).toUInt(), static_cast<uint32_t>(KEY_Z));
        QCOMPARE(keySpy.at(last).at(1).toUInt(), static_cast<uint32_t>(WL_KEYBOARD_KEY_STATE_RELEASED));

        // A key with diacritics options.
        sendKey(KEY_A, 10);

        QTRY_VERIFY(keySpy.count() >= 4);
        last = keySpy.count() - 1;
        secondLast = last - 1;

        QCOMPARE(keySpy.at(secondLast).at(0).toUInt(), static_cast<uint32_t>(KEY_A));
        QCOMPARE(keySpy.at(secondLast).at(1).toUInt(), static_cast<uint32_t>(WL_KEYBOARD_KEY_STATE_PRESSED));
        QCOMPARE(keySpy.at(last).at(0).toUInt(), static_cast<uint32_t>(KEY_A));
        QCOMPARE(keySpy.at(last).at(1).toUInt(), static_cast<uint32_t>(WL_KEYBOARD_KEY_STATE_RELEASED));
    }

    // Starting another diacritic key must balance an outstanding synthetic
    // repeat press even before the old physical key is released.
    void testRepeatRolloverBalancesPress()
    {
        auto *context = m_inputMethod->context();
        auto *keyboard = context->keyboard();
        QSignalSpy keySpy(context, &InputMethodContext::keyReceived);
        keyboard->sendKey(KEY_A, WL_KEYBOARD_KEY_STATE_PRESSED);
        wl_display_flush_clients(m_compositor->display());
        // Initial click, then the synthetic repeat press after hold + grace.
        QTRY_VERIFY_WITH_TIMEOUT(keySpy.count() >= 3, 5000);
        QCOMPARE(keySpy.at(2).at(0).toUInt(), uint32_t(KEY_A));
        QCOMPARE(keySpy.at(2).at(1).toUInt(), uint32_t(WL_KEYBOARD_KEY_STATE_PRESSED));

        keyboard->sendKey(KEY_E, WL_KEYBOARD_KEY_STATE_PRESSED);
        wl_display_flush_clients(m_compositor->display());
        QTest::qWait(100);
        // Check before sending either physical release: cancellation itself
        // must end the synthetic press, not rely on a future physical event.
        bool released = false;
        for (int i = 3; i < keySpy.count(); ++i) {
            const auto &event = keySpy.at(i);
            released |= event.at(0).toUInt() == KEY_A && event.at(1).toUInt() == WL_KEYBOARD_KEY_STATE_RELEASED;
        }
        keyboard->sendKey(KEY_A, WL_KEYBOARD_KEY_STATE_RELEASED);
        keyboard->sendKey(KEY_E, WL_KEYBOARD_KEY_STATE_RELEASED);
        wl_display_flush_clients(m_compositor->display());
        QTest::qWait(100);
        QVERIFY2(released, "Rollover left the synthetic repeat key pressed");
    }

    void cleanupTestCase()
    {
        if (m_child) {
            bool stopped = true;
            if (m_child->state() != QProcess::NotRunning) {
                QSignalSpy finished(m_child.get(), &QProcess::finished);
                m_child->terminate();
                // Keep the mock's D-Bus/Wayland services responsive while the
                // actual keyboard restores its state and destroys its windows.
                stopped = m_child->state() == QProcess::NotRunning || finished.wait(5000);
                if (!stopped) {
                    m_child->kill();
                    m_child->waitForFinished(1000);
                }
            }
            const bool normal = stopped && m_child->exitStatus() == QProcess::NormalExit && m_child->exitCode() == 0;
            disconnect(m_child.get(), nullptr, this, nullptr);
            m_child.reset();
            QVERIFY2(stopped, "The keyboard did not finish orderly shutdown");
            QVERIFY2(normal, "The keyboard exited abnormally");
        }
    }

private:
    /**
     * Helper that simulates compositor-side auto-repeat when a key is held.
     *
     * Sends a press event, waits for the initial repeat delay, then sends multiple
     * additional press events at the repeat interval to simulate compositor-side
     * key repeat, and finally sends the release event.
     *
     * Unlike sendKey() which produces a single press/release tap, this helper
     * generates the sequence of events that a real compositor sends during
     * auto-repeat, testing the repeat pipeline end-to-end.
     *
     * @param key The Linux scancode of the key (KEY_* from input-event-codes.h)
     * @param repeatDelayMs Initial delay before repeat begins
     * @param repeatIntervalMs Interval between repeat events
     * @param repeatCount Number of repeat events to send
     */
    void sendKeyWithRepeat(int key, int repeatDelayMs = 300, int repeatIntervalMs = 50, int repeatCount = 5)
    {
        auto keyboard = m_inputMethod->context()->keyboard();
        Q_ASSERT(keyboard);

        // First press
        keyboard->sendKey(key, WL_KEYBOARD_KEY_STATE_PRESSED);
        wl_display_flush_clients(m_compositor->display());

        // Wait for the initial repeat delay
        QTest::qWait(repeatDelayMs);

        // Send repeat events at the repeat interval using the dedicated
        // WL_KEYBOARD_KEY_STATE_REPEATED state, which informs the client that these are
        // auto-repeat presses rather than independent key-down events.
        for (int i = 0; i < repeatCount; ++i) {
            keyboard->sendKey(key, WL_KEYBOARD_KEY_STATE_REPEATED);
            wl_display_flush_clients(m_compositor->display());
            QTest::qWait(repeatIntervalMs);
        }

        keyboard->sendKey(key, WL_KEYBOARD_KEY_STATE_RELEASED);
        wl_display_flush_clients(m_compositor->display());
    }

    QTemporaryDir m_runtimeDir;
    QTemporaryDir m_home;
    QString m_socketPath;

    std::unique_ptr<QWaylandCompositor> m_compositor;
    std::unique_ptr<QWindow> m_outputWindow;
    std::unique_ptr<QWaylandOutput> m_output;
    QWaylandSeat *m_seat = nullptr;
    std::unique_ptr<QWaylandXdgShell> m_xdgShell;
    std::unique_ptr<InputMethodV1> m_inputMethod;
    std::unique_ptr<InputPanelV1> m_inputPanel;
    std::unique_ptr<TestLayerShell> m_layerShell;
    TestScreenSaver m_screenSaver;
    TestTouchActivation m_touchActivation;
    TestVirtualKeyboard m_virtualKeyboard;
    std::unique_ptr<QProcess> m_child;
};

QTEST_MAIN(MockInputMethodCompositorTest)

#include "mockinputmethodcompositor.moc"
