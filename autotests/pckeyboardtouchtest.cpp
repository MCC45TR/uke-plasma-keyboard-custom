// SPDX-License-Identifier: GPL-2.0-or-later
#include "touchdeliverypolicy.h"
#include <QColor>
#include <QFile>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickView>
#include <QSet>
#include <QTemporaryDir>
#include <QtTest>
#include <KLocalizedQmlContext>
#include <qqml.h>
#include <qpa/qwindowsysteminterface.h>
#include <cmath>
#include <tuple>

// Only record the bridge and replace theme/settings dependencies. All key
// geometry, touch, pending-order, cancellation and popup logic is production QML.
class PcBridge final:public QObject {
    Q_OBJECT
    Q_PROPERTY(double pixelsPerMm READ pixelsPerMm CONSTANT)
    Q_PROPERTY(QVariantMap options READ options CONSTANT)
    Q_PROPERTY(QVariantList rows READ rows CONSTANT)
    Q_PROPERTY(QVariantList navigationRows READ navigationRows CONSTANT)
    Q_PROPERTY(bool modifierActive READ modifierActive NOTIFY changed)
    Q_PROPERTY(int keyRevision READ revision NOTIFY changed)
public:
    QList<QPair<uint,bool>> events;QSet<uint> held;int m_revision=0;
    double pixelsPerMm()const{return 3;}
    int revision()const{return m_revision;}
    bool modifierActive()const{return held.contains(125)||held.contains(42);}
    QVariantMap options()const{return {{QStringLiteral("gapMm"),1},{QStringLiteral("keyHeightMm"),11},{QStringLiteral("keyWidthMm"),14},{QStringLiteral("keyPreview"),false},{QStringLiteral("spaceGesture"),true},{QStringLiteral("deleteGesture"),true}};}
    static QVariantList row(std::initializer_list<std::pair<uint,const char*>> keys){
        QVariantList result;for(const auto& [code,label]:keys)result.append(QVariantMap{{QStringLiteral("code"),code},{QStringLiteral("label"),QString::fromUtf8(label)},{QStringLiteral("units"),1},{QStringLiteral("modifier"),code==125||code==42},{QStringLiteral("dynamic"),false}});return result;
    }
    QVariantList rows()const{return {QVariant(row({{125,"Meta"},{42,"Shift"}})),QVariant(row({{30,"a"},{48,"b"}})),QVariant(row({{57,"Space"},{14,"Backspace"}}))};}
    QVariantList navigationRows()const{return {QVariant(row({{110,"Ins"},{102,"Home"},{104,"PgUp"}})),QVariant(row({{111,"Del"},{107,"End"},{109,"PgDn"}})),QVariant(row({{0,""},{103,"Up"},{0,""}})),QVariant(row({{105,"Left"},{108,"Down"},{106,"Right"}}))};}
    Q_INVOKABLE QString keyLabel(uint,const QString& fallback)const{return fallback;}
    Q_INVOKABLE bool latched(uint code)const{return held.contains(code);}
    Q_INVOKABLE QStringList alternatives(const QString&)const{return {};}
    Q_INVOKABLE void key(uint code,bool pressed){events.append({code,pressed});if(pressed)held.insert(code);else held.remove(code);++m_revision;Q_EMIT changed();}
    Q_INVOKABLE void moveCursor(int){}
    Q_INVOKABLE void deleteWords(int){}
Q_SIGNALS:
    void changed();
};
class PcTheme final:public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* current READ current CONSTANT)
    Q_PROPERTY(double buttonRadius READ radius CONSTANT)
    Q_PROPERTY(QString fontFamily READ family CONSTANT)
public:
    QObject* current(){return this;}
    double radius()const{return 3;}
    QString family()const{return QStringLiteral("Sans Serif");}
    Q_INVOKABLE QVariantMap keyOutlineFor(const QString&)const{return {{QStringLiteral("width"),0},{QStringLiteral("color"),QStringLiteral("transparent")}};}
    Q_INVOKABLE double keyShadowFor(const QString&)const{return 0;}
    Q_INVOKABLE QColor keyColorFor(const QString&,const QString&)const{return QColor(Qt::gray);}
    Q_INVOKABLE QColor categoryActiveColorFor(const QString&)const{return QColor(Qt::darkGray);}
    Q_INVOKABLE QColor keyTextColorFor(const QString&)const{return QColor(Qt::white);}
};
class PcSettings final:public QObject {
    Q_OBJECT
    Q_PROPERTY(bool soundEnabled READ disabled CONSTANT)
    Q_PROPERTY(bool vibrationEnabled READ disabled CONSTANT)
    Q_PROPERTY(bool swipeTypingEnabled READ disabled CONSTANT)
    Q_PROPERTY(bool diacriticsPopupEnabled READ enabled CONSTANT)
public:
    bool disabled()const{return false;}
    bool enabled()const{return true;}
};
class PcKeyboardTouchTest final:public QObject {
    Q_OBJECT
private Q_SLOTS:
    void actualPhysicalChordsFastRolloverAndResponsiveNavigation(){
        PcBridge bridge;PcTheme theme,constants;PcSettings settings;
        qmlRegisterSingletonInstance("UkePcTouchTest",1,0,"Laptop",&bridge);
        qmlRegisterSingletonInstance("UkePcTouchTest",1,0,"Theme",&theme);
        qmlRegisterSingletonInstance("UkePcTouchTest",1,0,"BreezeConstants",&constants);
        qmlRegisterSingletonInstance("UkePcTouchTest",1,0,"PlasmaKeyboardSettings",&settings);
        QVERIFY(qmlProtectModule("UkePcTouchTest",1));
        QQuickView window;window.resize(900,400);KLocalization::setupLocalizedContext(window.engine());
        QFile source(QStringLiteral(PC_KEYBOARD_SOURCE));QVERIFY(source.open(QIODevice::ReadOnly));auto data=source.readAll();
        const QByteArray bridgeImport="import org.uke.plasma.keyboard.lib as Keyboard",settingsImport="import org.uke.plasma.keyboard\n";
        QCOMPARE(data.count(bridgeImport),1);QCOMPARE(data.count(settingsImport),1);
        data.replace(bridgeImport,"import UkePcTouchTest 1.0 as Keyboard");data.replace(settingsImport,"import UkePcTouchTest 1.0\n");
        QTemporaryDir directory;QVERIFY(directory.isValid());QQmlComponent component(window.engine());
        component.setData(data,QUrl::fromLocalFile(directory.filePath(QStringLiteral("PcKeyboard.qml"))));
        QScopedPointer<QObject> object(component.create());auto* keyboard=qobject_cast<QQuickItem*>(object.data());QVERIFY2(keyboard,qPrintable(component.errorString()));
        keyboard->setParentItem(window.contentItem());keyboard->setSize(QSizeF(800,300));keyboard->setProperty("auxiliaryAllowed",true);
        window.show();QVERIFY(QTest::qWaitForWindowExposed(&window));QCoreApplication::processEvents();
        const auto item=[&](uint code)->QQuickItem*{
            const auto name=QStringLiteral("ukeKey_%1").arg(code);QList<QQuickItem*> pending{keyboard};
            while(!pending.isEmpty()){auto* next=pending.takeLast();if(next->objectName()==name)return next;pending.append(next->childItems());}
            return nullptr;
        };
        for(uint code:{30U,48U,125U,42U,103U,105U,108U,106U,104U,109U})QVERIFY2(item(code),qPrintable(QStringLiteral("Missing visual key %1").arg(code)));
        const auto* device=QTest::createTouchDevice();ulong timestamp=1;
        const auto centre=[&](uint code){auto* key=item(code);return key->mapToItem(window.contentItem(),QPointF(key->width()/2,key->height()/2));};
        const auto event=[&](std::initializer_list<std::tuple<int,QEventPoint::State,QPointF>> states){
            QList<QWindowSystemInterface::TouchPoint> points;
            for(const auto& [id,state,position]:states){QWindowSystemInterface::TouchPoint p;p.id=id;p.state=state;p.area=QRectF(QPointF(window.mapToGlobal(QPoint()))+position-QPointF(1,1),QSizeF(2,2));p.pressure=state==QEventPoint::State::Released?0:1;points.append(p);}
            QWindowSystemInterface::handleTouchEvent(&window,timestamp++,device,points);QWindowSystemInterface::sendWindowSystemEvents(QEventLoop::AllEvents);
        };
        using State=QEventPoint::State;
        const auto a=centre(30),b=centre(48),meta=centre(125);
        event({{0,State::Pressed,a}});QVERIFY(bridge.events.isEmpty());
        event({{0,State::Stationary,a},{1,State::Pressed,b}});
        const QList<QPair<uint,bool>> firstPress{{30,true}};QCOMPARE(bridge.events,firstPress);
        event({{0,State::Stationary,a},{1,State::Released,b}});event({{0,State::Released,a}});
        const QList<QPair<uint,bool>> rollover{{30,true},{48,true},{48,false},{30,false}};QCOMPARE(bridge.events,rollover);QVERIFY(bridge.held.isEmpty());
        bridge.events.clear();event({{0,State::Pressed,meta}});event({{0,State::Stationary,meta},{1,State::Pressed,a}});
        event({{0,State::Released,meta},{1,State::Stationary,a}});event({{1,State::Released,a}});
        const QList<QPair<uint,bool>> chord{{125,true},{30,true},{125,false},{30,false}};QCOMPARE(bridge.events,chord);QVERIFY(bridge.held.isEmpty());
        bridge.events.clear();auto* letter=item(30);const auto low=letter->mapToItem(window.contentItem(),QPointF(letter->width()/2,letter->height()-0.5));
        event({{0,State::Pressed,low}});event({{0,State::Updated,low+QPointF(0,0.1)}});event({{0,State::Released,low+QPointF(0,0.1)}});
        const QList<QPair<uint,bool>> tap{{30,true},{30,false}};QCOMPARE(bridge.events,tap);
        auto* stableLetter=item(30);bridge.events.clear();event({{0,State::Pressed,centre(30)}});
        keyboard->setSize(QSizeF(600,260));QCoreApplication::processEvents();QCOMPARE(item(30),stableLetter);
        event({{0,State::Released,centre(30)}});QCOMPARE(bridge.events,tap);QVERIFY(bridge.held.isEmpty());
        for(int width:{430,600,800}){
            keyboard->setWidth(width);QCoreApplication::processEvents();
            const auto up=centre(103),down=centre(108),left=centre(105),right=centre(106);
            QVERIFY(up.y()<down.y());QVERIFY(std::abs(up.x()-down.x())<0.01);QVERIFY(left.x()<down.x()&&down.x()<right.x());
            QVERIFY(centre(104).y()<up.y()&&centre(109).y()<up.y());
            for(uint code:{30U,48U,125U,103U,105U,108U,106U,104U,109U}){auto* key=item(code);QVERIFY(key->x()>=0&&key->x()+key->width()<=width+0.01);QVERIFY(key->y()>=0&&key->y()+key->height()<=keyboard->height()+0.01);}
        }
    }
};
int main(int argc,char** argv){UkeKeyboard::configureTouchDelivery();QGuiApplication app(argc,argv);PcKeyboardTouchTest test;return QTest::qExec(&test,argc,argv);}
#include "pckeyboardtouchtest.moc"
