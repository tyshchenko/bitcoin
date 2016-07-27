// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_MEMPOOLSTATS_H
#define BITCOIN_QT_MEMPOOLSTATS_H

#include <QWidget>
#include <QGraphicsLineItem>

class ClientModel;

namespace Ui {
    class MempoolStats;
}

class MempoolStats : public QWidget
{
    Q_OBJECT

public:
    MempoolStats(QWidget *parent = 0);
    ~MempoolStats();

    void setClientModel(ClientModel *model);

public Q_SLOTS:
    void drawChart();

private:
    Ui::MempoolStats *ui;
    ClientModel *clientModel;

    virtual void resizeEvent(QResizeEvent *event);
    QGraphicsLineItem *line;
    QGraphicsTextItem *titleItem;
    QGraphicsTextItem *txCountItem;
    QGraphicsTextItem *minFeeItem;
    QGraphicsScene *scene;
    QVector<QGraphicsItem*> redrawItems;
};

#endif // BITCOIN_QT_MEMPOOLSTATS_H
