// Copyright (c) 2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpctextedit.h"

RPCTextEdit::RPCTextEdit(QWidget *parent)
{

}

void RPCTextEdit::copy()
{
    QTextEdit::copy();
}

void RPCTextEdit::keyPressEvent(QKeyEvent *e)
{
    if(e->key() == Qt::Key_Backtab){
        //Do something
    }
    else if(e->key() == Qt::Key_Return || e->key() == Qt::Key_Tab || e->key() == Qt::Key_Enter){
        //Do something
    }
    else{
        QTextEdit::keyPressEvent(e);
    }
}