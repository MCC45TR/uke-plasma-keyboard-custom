/*
 SPDX-FileCopyrightText: 2026 Chechulin Serhii
 SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#pragma once
#include <QObject>
#include <QElapsedTimer>
#include <QPointF>
#include <QSet>
#include <QTimer>
// Selected-input-method-only compositor notifications. No evdev permissions.
class TouchHoldWatcher : public QObject {
 Q_OBJECT
public:
 static constexpr int defaultThresholdMs=600;
 explicit TouchHoldWatcher(QObject *parent=nullptr);
 ~TouchHoldWatcher() override=default;
 TouchHoldWatcher(const TouchHoldWatcher&)=delete;
 TouchHoldWatcher&operator=(const TouchHoldWatcher&)=delete;
 bool arm(int thresholdMs=defaultThresholdMs);
 bool watchContinuously(int thresholdMs=defaultThresholdMs);
 void disarm();
 bool isArmed()const{return m_armed;}
 bool touchIsDown()const{return !m_contacts.isEmpty();}
 QPointF normalizedPosition()const{return m_position;}
 bool takeCompletedLongPress(int thresholdMs);
Q_SIGNALS:
 void longPress();
private Q_SLOTS:
 void touch(int phase,int id,double x,double y,double slopX,double slopY);
private:
 QSet<int>m_contacts;
 QPointF m_position,m_start;
 QElapsedTimer m_elapsed,m_released;
 QTimer m_timer;
 int m_threshold=defaultThresholdMs,m_completed=-1;
 bool m_armed=false,m_persistent=false,m_moved=false;
};
