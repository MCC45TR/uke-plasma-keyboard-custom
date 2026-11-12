// SPDX-License-Identifier: GPL-2.0-or-later
#include "usbkeyboardpolicy.h"
#include <QtTest>

class Device final : public KWin::InputDevice {
    Q_OBJECT
    Q_PROPERTY(bool alphaNumericKeyboard MEMBER alphanumeric)
public:
    QString path=QStringLiteral("/sys/devices/platform/usb1/1-1/input/input9/event9");
    quint32 bus=BUS_USB;
    bool keyboard=true,alphanumeric=true,enabled=true;
    QString name() const override { return QStringLiteral("Synthetic keyboard"); }
    QString sysPath() const override { return path; }
    quint32 busType() const override { return bus; }
    bool isEnabled() const override { return enabled; }
    void setEnabled(bool value) override {enabled=value;}
    bool isKeyboard() const override { return keyboard; }
    bool isPointer() const override { return false; }
    bool isTouchpad() const override { return false; }
    bool isTouch() const override { return false; }
    bool isTabletTool() const override { return false; }
    bool isTabletPad() const override { return false; }
    bool isTabletModeSwitch() const override { return false; }
    bool isLidSwitch() const override { return false; }
};
class UsbKeyboardPolicyTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void physicalUsbAndReceiverBlock() {
        Device device;QVERIFY(UkeKeyboard::isPhysicalUsbKeyboard(&device));
        // Disabling input events does not physically unplug the keyboard.
        device.setEnabled(false);QVERIFY(UkeKeyboard::isPhysicalUsbKeyboard(&device));
        QVERIFY(!UkeKeyboard::isPhysicalUsbKeyboard(nullptr));
    }
    void pogoBluetoothAndVirtualDoNotBlock() {
        Device device;
        for (const auto bus : {BUS_I2C,BUS_HOST,BUS_SPI,BUS_BLUETOOTH}) {
            device.bus=bus;QVERIFY(!UkeKeyboard::isPhysicalUsbKeyboard(&device));
        }
        device.bus=BUS_USB;device.path=QStringLiteral("/sys/devices/virtual/input/input9/event9");
        QVERIFY(!UkeKeyboard::isPhysicalUsbKeyboard(&device));
        device.path.clear();QVERIFY(!UkeKeyboard::isPhysicalUsbKeyboard(&device));
    }
    void mouseAndMediaButtonsDoNotBlock() {
        Device device;device.keyboard=false;QVERIFY(!UkeKeyboard::isPhysicalUsbKeyboard(&device));
        device.keyboard=true;device.alphanumeric=false;QVERIFY(!UkeKeyboard::isPhysicalUsbKeyboard(&device));
    }
};
QTEST_GUILESS_MAIN(UsbKeyboardPolicyTest)
#include "usbkeyboardpolicytest.moc"
