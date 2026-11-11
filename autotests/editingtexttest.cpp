// SPDX-License-Identifier: GPL-2.0-or-later
#include "editingtext.h"
#include <QtTest>
class LocalEditor final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString text MEMBER text)
    Q_PROPERTY(int cursorPosition MEMBER position)
    Q_PROPERTY(int selectionStart MEMBER start)
    Q_PROPERTY(int selectionEnd MEMBER end)
public:
    QString text;
    int position=0,start=0,end=0;
    Q_INVOKABLE void remove(int first,int last){text.remove(first,last-first);start=end=first;}
};
class EditingTextTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void cursorUsesUnicodeBoundaries(){
        LocalEditor editor;editor.text=QString::fromUtf8("A😀B");editor.position=4;
        UkeKeyboard::editLocalText(&editor,-2,false);QCOMPARE(editor.position,1);
        UkeKeyboard::editLocalText(&editor,1,false);QCOMPARE(editor.position,3);
        QCOMPARE(editor.text,QString::fromUtf8("A😀B"));
    }
    void wordsRemainInTheLocalEditor(){
        LocalEditor editor;editor.text=QString::fromUtf8("merhaba dünya");editor.position=editor.text.size();
        UkeKeyboard::editLocalText(&editor,1,true);
        QCOMPARE(editor.text,QStringLiteral("merhaba "));QCOMPARE(editor.position,8);
        UkeKeyboard::editLocalText(&editor,1,true);QVERIFY(editor.text.isEmpty());QCOMPARE(editor.position,0);
    }
    void selectionIsDeletedOnce(){
        LocalEditor editor;editor.text=QStringLiteral("one two three");editor.position=7;editor.start=4;editor.end=7;
        UkeKeyboard::editLocalText(&editor,3,true);
        QCOMPARE(editor.text,QStringLiteral("one  three"));QCOMPARE(editor.position,4);
    }
    void invalidLocalTargetDoesNothing(){
        QObject target;UkeKeyboard::editLocalText(&target,10,true);
        QVERIFY(!target.property("text").isValid());
        UkeKeyboard::editLocalText(nullptr,-1,false);
    }
};
QTEST_MAIN(EditingTextTest)
#include "editingtexttest.moc"
