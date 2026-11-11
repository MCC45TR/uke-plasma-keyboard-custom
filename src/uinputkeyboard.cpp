// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
// Existing call sites use this compatibility facade; no kernel device is opened.
#include "uinputkeyboard.h"
#include "laptop/laptopcontroller.h"
UinputKeyboard::UinputKeyboard(QObject *parent):QObject(parent){}
UinputKeyboard::~UinputKeyboard()=default;
bool UinputKeyboard::isValid()const{return LaptopController::instance()&&LaptopController::instance()->backendReady();}
void UinputKeyboard::pressKey(int code){if(isValid())LaptopController::instance()->backendCall(QStringLiteral("Key"),{uint(code),true});}
void UinputKeyboard::releaseKey(int code){if(isValid())LaptopController::instance()->backendCall(QStringLiteral("Key"),{uint(code),false});}
void UinputKeyboard::enqueue(int,int){}
void UinputKeyboard::flushNextEvent(){}
void UinputKeyboard::writeEvent(int,int){}
