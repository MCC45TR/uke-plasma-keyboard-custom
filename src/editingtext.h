// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QObject>
#include <QTextCursor>
#include <QTextDocument>
#include <QVariant>
#include <algorithm>
#include <cstdlib>
namespace UkeKeyboard {
// Local toolbar editors never fall through to the external input context.
inline void editLocalText(QObject *editor,int amount,bool words) {
    if(!editor||!editor->property("text").isValid()||!editor->property("cursorPosition").isValid())return;
    const auto text=editor->property("text").toString();
    const int position=std::clamp(editor->property("cursorPosition").toInt(),0,int(text.size()));
    QTextDocument document;document.setPlainText(text);QTextCursor cursor(&document);cursor.setPosition(position);
    if(words){
        if(amount<=0)return;
        int start=std::clamp(editor->property("selectionStart").toInt(),0,int(text.size()));
        int end=std::clamp(editor->property("selectionEnd").toInt(),0,int(text.size()));
        if(start==end){cursor.movePosition(QTextCursor::PreviousWord,QTextCursor::KeepAnchor,std::min(amount,32));start=cursor.selectionStart();end=cursor.selectionEnd();}
        if(start>end)std::swap(start,end);
        if(start!=end)QMetaObject::invokeMethod(editor,"remove",Q_ARG(int,start),Q_ARG(int,end));
        editor->setProperty("cursorPosition",start);
    }else{
        const int count=std::abs(std::clamp(amount,-32,32));
        cursor.movePosition(amount<0?QTextCursor::PreviousCharacter:QTextCursor::NextCharacter,QTextCursor::MoveAnchor,count);
        editor->setProperty("cursorPosition",cursor.position());
    }
}
}
