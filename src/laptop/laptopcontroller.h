// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QObject>
#include <QDBusConnection>
#include <QElapsedTimer>
#include <QFileSystemWatcher>
#include <QPointer>
#include <QScreen>
#include <QSet>
#include <QVariantList>
#include <QVariantMap>
#include <KScreen/Config>
#include <KScreen/Output>
#include <memory>
#include <xkbcommon/xkbcommon.h>
class QScreen;
class QWindow;
class QDBusMessage;
class QDBusPendingCall;

class LaptopController final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.uke.PlasmaKeyboard.Control1")
    Q_PROPERTY(bool active READ active NOTIFY stateChanged)
    Q_PROPERTY(bool available READ available NOTIFY stateChanged)
    Q_PROPERTY(bool backendReady READ backendReady NOTIFY stateChanged)
    Q_PROPERTY(QString reason READ reason NOTIFY stateChanged)
    Q_PROPERTY(QScreen *tabletScreen READ tabletScreen NOTIFY stateChanged)
    Q_PROPERTY(double pixelsPerMm READ pixelsPerMm NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap options READ options NOTIFY optionsChanged)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY layoutChanged)
    Q_PROPERTY(QVariantList layouts READ layouts NOTIFY layoutChanged)
    Q_PROPERTY(QString layoutName READ layoutName NOTIFY layoutChanged)
    Q_PROPERTY(QString locale READ locale NOTIFY layoutChanged)
    Q_PROPERTY(bool complexLayout READ complexLayout NOTIFY layoutChanged)
    Q_PROPERTY(bool dictionaryAvailable READ dictionaryAvailable NOTIFY layoutChanged)
    Q_PROPERTY(QString dictionaryPackage READ dictionaryPackage NOTIFY layoutChanged)
    Q_PROPERTY(bool typing READ typing NOTIFY typingChanged)
    Q_PROPERTY(int keyRevision READ keyRevision NOTIFY keyStateChanged)
public:
    explicit LaptopController(QObject *parent=nullptr);
    ~LaptopController() override;
    static LaptopController *instance();
    bool active() const { return m_active; }
    bool available() const { return m_ready && m_available && !m_locked && !m_lidClosed; }
    bool backendReady() const { return m_ready; }
    QString reason() const { return m_reason; }
    QScreen *tabletScreen() const { return m_screen; }
    double pixelsPerMm() const;
    QVariantMap options() const;
    QVariantList rows() const;
    QVariantList layouts() const { return m_layouts; }
    QString layoutName() const;
    QString locale() const;
    bool complexLayout() const;
    bool dictionaryAvailable() const;
    QString dictionaryPackage() const;
    bool typing() const { return m_typing; }
    int keyRevision() const { return m_revision; }
    Q_INVOKABLE QString keyLabel(uint code,const QString &fallback) const;
    Q_INVOKABLE bool latched(uint code) const { return m_latched.contains(code)||m_lockedKeys.contains(code); }
    Q_INVOKABLE QStringList alternatives(const QString &text) const;
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void enter();
    Q_INVOKABLE void leave();
    Q_INVOKABLE void setOption(const QString &key,const QVariant &value);
    Q_INVOKABLE void reload();
    Q_INVOKABLE void key(uint code,bool pressed);
    Q_INVOKABLE void tap(uint code);
    Q_INVOKABLE void moveCursor(int steps);
    Q_INVOKABLE void deleteWords(int count);
    Q_INVOKABLE void releaseAll();
    Q_INVOKABLE void motion(double x,double y);
    Q_INVOKABLE void button(uint code,bool pressed);
    Q_INVOKABLE void scroll(double x,double y);
    Q_INVOKABLE void swipe(int fingers,int phase,double x,double y);
    Q_INVOKABLE void pinch(int phase,double scale,double angle,double x,double y);
    Q_INVOKABLE void selectLayout(int index);
    Q_INVOKABLE void nextLayout();
    Q_INVOKABLE void addLayout(const QString &layout,const QString &variant);
    Q_INVOKABLE QVariantList allLayouts() const;
    Q_INVOKABLE QVariantList variants(const QString &layout) const;
    Q_INVOKABLE void openPreferences();
    Q_INVOKABLE void tool(const QString &name);
    Q_INVOKABLE void requestTopology();
    Q_INVOKABLE QVariantMap status() const;
    Q_INVOKABLE QVariantList emojiMatches(const QString &query) const;
    QDBusPendingCall backendCall(const QString &method,const QVariantList &args={}) const;
Q_SIGNALS:
    void stateChanged();
    void optionsChanged();
    void layoutChanged();
    void typingChanged();
    void keyStateChanged();
    void preferencesRequested();
private Q_SLOTS:
    void backendStatic(bool active);
    void systemLayout(uint index);
    void propertiesChanged(const QString &iface,const QVariantMap &changed,const QStringList &invalidated);
    void refreshRemote();
private:
    void connectBackend();
    void updateTopology();
    void loadLayouts();
    void rebuildKeymap();
    void refreshProfile(bool restore=false);
    void setRawKey(uint code,bool pressed);
    void updateModifiers();
    void markTyping();
    static QVariantMap defaults();
    static LaptopController *s_instance;
    QDBusConnection m_bus;
    KScreen::ConfigPtr m_config;
    QPointer<QScreen> m_screen;
    QString m_reason,m_tabletName,m_savedMode,m_appliedMode;
    KScreen::Output::VrrPolicy m_savedVrr=KScreen::Output::VrrPolicy::Automatic;
    KScreen::Output::VrrPolicy m_appliedVrr=KScreen::Output::VrrPolicy::Automatic;
    QVariantMap m_options;
    QVariantList m_layouts;
    QStringList m_layoutCodes,m_variants;
    int m_layoutIndex=0;
    bool m_active=false,m_ready=false,m_available=false,m_locked=true,m_lidClosed=true,m_typing=false,m_profileBusy=false;
    bool m_preferencesOnly=false;
    double m_remoteDensity=0;
    QSet<uint> m_held,m_latched,m_lockedKeys;
    QSet<uint> m_modifierUsed;
    QHash<uint,qint64> m_pressTimes,m_tapTimes;
    QElapsedTimer m_clock;
    quint64 m_typingGeneration=0;
    int m_revision=0;
    QFileSystemWatcher m_files;
    struct XkbDeleter { void operator()(xkb_context *p) const {xkb_context_unref(p);} void operator()(xkb_keymap *p) const {xkb_keymap_unref(p);} void operator()(xkb_state *p) const {xkb_state_unref(p);} };
    std::unique_ptr<xkb_context,XkbDeleter> m_xkb;
    std::unique_ptr<xkb_keymap,XkbDeleter> m_keymap;
    std::unique_ptr<xkb_state,XkbDeleter> m_state;
};
