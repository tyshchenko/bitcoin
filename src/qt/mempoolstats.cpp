// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mempoolstats.h"
#include "ui_mempoolstats.h"

#include "clientmodel.h"

#include "stats.h"

MempoolStats::MempoolStats(QWidget *parent) :
QWidget(parent, Qt::Window),
scene(0),
titleItem(0),
txCountItem(0),
minFeeItem(0),
clientModel(0),
ui(new Ui::MempoolStats)
{
    ui->setupUi(this);
    if (parent) {
        parent->installEventFilter(this);
        raise();
    }

    scene = new QGraphicsScene();
    ui->graphicsView->setScene(scene);
    ui->graphicsView->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

    if (clientModel)
        drawChart();
}

void MempoolStats::setClientModel(ClientModel *model)
{
    clientModel = model;

    if (model)
        connect(model, SIGNAL(mempoolStatsDidUpdate()), this, SLOT(drawChart()));
}

void MempoolStats::drawChart()
{
    if (!titleItem)
    {
        titleItem = scene->addText("Mempool Statistics");
        txCountItem = scene->addText("Tx Count");
        minFeeItem = scene->addText("MinFee");
        txCountItem->setPos(titleItem->boundingRect().width()+20, txCountItem->pos().y());
        minFeeItem->setPos(titleItem->boundingRect().width()+20, txCountItem->pos().y()+txCountItem->boundingRect().height()+5);
    }

    for (QGraphicsItem * item : redrawItems)
    {
        scene->removeItem(item);
        delete item;
    }
    redrawItems.clear();

    QDateTime fromDateTime;
    QDateTime toDateTime;
    fromDateTime = fromDateTime.addSecs(-3600); //-1h

    mempoolSamples_t vSamples = clientModel->getMempoolStatsInRange(fromDateTime, toDateTime);
    if (!vSamples.size())
        return;

    txCountItem->setPlainText("TX Count: "+QString::number(vSamples.back().txCount));
    minFeeItem->setPlainText("MinFee/K: "+QString::number(vSamples.back().minFeePerK));

    qreal labelLeftSize = 30;
    qreal labelRightSize = 30;
    qreal paddingLeft = 30+labelLeftSize;
    qreal paddingRight = 30+labelRightSize;
    qreal paddingTop = 10;
    qreal paddingTopLabel = 80;
    qreal paddingBottom = 40;
    qreal labelHeight = 15;
    qreal bottom = ui->graphicsView->size().height()-paddingBottom;
    qreal maxwidth = ui->graphicsView->size().width()-paddingLeft-paddingRight;
    qreal maxheightG = ui->graphicsView->size().height()-paddingTop-paddingTopLabel-labelHeight;
    float paddingTopSizeFactor = 0.9;
    qreal maxheightC = maxheightG*paddingTopSizeFactor;
    qreal step = maxwidth/(double)vSamples.size();
    int64_t maxDynMemUsage = 0;
    int64_t minDynMemUsage = std::numeric_limits<int64_t>::max();
    int64_t maxTxCount = 0;
    int64_t minTxCount = std::numeric_limits<int64_t>::max();
    int64_t maxMinFee = 0;
    uint32_t maxTimeDetla = vSamples.back().timeDelta;
    for(const struct CStatsMempoolSample &sample : vSamples)
    {
        if (sample.dynMemUsage > maxDynMemUsage)
            maxDynMemUsage = sample.dynMemUsage;

        if (sample.dynMemUsage < minDynMemUsage)
            minDynMemUsage = sample.dynMemUsage;

        if (sample.txCount > maxTxCount)
            maxTxCount = sample.txCount;

        if (sample.txCount < minTxCount)
            minTxCount = sample.txCount;

        if (sample.minFeePerK > maxMinFee)
            maxMinFee = sample.minFeePerK;
    }

    qreal currentX = paddingLeft;
    QPainterPath dynMemUsagePath(QPointF(currentX, bottom));
    QPainterPath txCountPath(QPointF(currentX, bottom));
    QPainterPath minFeePath(QPointF(currentX, bottom));

    for(const struct CStatsMempoolSample &sample : vSamples)
    {
        qreal xPos = maxTimeDetla > 0 ? maxwidth/maxTimeDetla*sample.timeDelta : maxwidth/(double)vSamples.size();
        if (sample.timeDelta == vSamples.front().timeDelta)
        {
            dynMemUsagePath.moveTo(paddingLeft+xPos, bottom-maxheightC/(maxDynMemUsage-minDynMemUsage)*sample.dynMemUsage);
            txCountPath.moveTo(paddingLeft+xPos, bottom-maxheightC/(maxTxCount-minTxCount)*sample.txCount);
            minFeePath.moveTo(paddingLeft+xPos, bottom-maxheightC/maxMinFee*sample.minFeePerK);
        }
        else
        {
            dynMemUsagePath.lineTo(paddingLeft+xPos, bottom-maxheightC/(maxDynMemUsage-minDynMemUsage)*sample.dynMemUsage);
            txCountPath.lineTo(paddingLeft+xPos, bottom-maxheightC/(maxTxCount-minTxCount)*sample.txCount);
            minFeePath.lineTo(paddingLeft+xPos, bottom-maxheightC/maxMinFee*sample.minFeePerK);
        }
    }

    QPainterPath dynMemUsagePathFill(dynMemUsagePath);

    dynMemUsagePathFill.lineTo(paddingLeft+maxwidth, bottom);
    dynMemUsagePathFill.lineTo(paddingLeft, bottom);

    QPainterPath dynMemUsageGridPath(QPointF(currentX, bottom));

    // draw horizontal grid
    int amountOfLinesH = 5;
    QFont gridFont;
    gridFont.setPointSize(8);
    for (int i=0; i < amountOfLinesH; i++)
    {
        qreal lY = bottom-i*(maxheightG/(amountOfLinesH-1));
        dynMemUsageGridPath.moveTo(paddingLeft, lY);
        dynMemUsageGridPath.lineTo(paddingLeft+maxwidth, lY);

        size_t gridDynSize = (float)i*(maxDynMemUsage-minDynMemUsage)/(amountOfLinesH-1);
        size_t gridTxCount = (float)i*(maxTxCount-minTxCount)/(amountOfLinesH-1);

        QString labelAtLine;
        if (gridDynSize < 1000000)
            labelAtLine = QString::number(gridDynSize/1000.0, 'f', 1) + " KB";
        else
            labelAtLine = QString::number(gridDynSize/1000000.0, 'f', 1) + " MB";

        QGraphicsTextItem *itemDynSize = scene->addText(labelAtLine, gridFont);
        QGraphicsTextItem *itemTxCount = scene->addText(QString::number(gridTxCount), gridFont);

        itemDynSize->setPos(paddingLeft-itemDynSize->boundingRect().width(), lY-(itemDynSize->boundingRect().height()/2));
        itemTxCount->setPos(paddingLeft+maxwidth, lY-(itemDynSize->boundingRect().height()/2));
        redrawItems.append(itemDynSize);
        redrawItems.append(itemTxCount);
    }

    // draw vertical grid
    int amountOfLinesV = 4;
    QDateTime drawTime(fromDateTime);
    std::string fromS = fromDateTime.toString().toStdString();
    std::string toS = toDateTime.toString().toStdString();
    qint64 secsTotal = fromDateTime.secsTo(toDateTime);
    for (int i=1; i <= amountOfLinesV; i++)
    {
        qreal lX = i*(maxwidth/(amountOfLinesV));
        dynMemUsageGridPath.moveTo(paddingLeft+lX, bottom);
        dynMemUsageGridPath.lineTo(paddingLeft+lX, bottom-maxheightG);

        QGraphicsTextItem *item = scene->addText(drawTime.toString("HH:mm"), gridFont);
        item->setPos(paddingLeft+lX-(item->boundingRect().width()/2), bottom);
        redrawItems.append(item);
        qint64 step = secsTotal/amountOfLinesV;
        drawTime = drawTime.addSecs(step);
    }

    // materialize path
    QPen gridPen(QColor(100,100,100, 200), 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    redrawItems.append(scene->addPath(dynMemUsageGridPath, gridPen));


    QLinearGradient gradient(currentX, bottom, currentX, 0);
    gradient.setColorAt(1.0,	QColor(15,68,113, 250));
    gradient.setColorAt(0,QColor(255,255,255,0));
    QBrush graBru(gradient);

    QPen linePenBlue(QColor(15,68,113, 250), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    QPen linePenRed(QColor(188,49,62, 250), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    QPen linePenGreen(QColor(49,188,62, 250), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    redrawItems.append(scene->addPath(dynMemUsagePath, linePenBlue));
    redrawItems.append(scene->addPath(txCountPath, linePenRed));
    redrawItems.append(scene->addPath(minFeePath, linePenGreen));
    redrawItems.append(scene->addPath(dynMemUsagePathFill, QPen(Qt::NoPen), graBru));
}

// We override the virtual resizeEvent of the QWidget to adjust tables column
// sizes as the tables width is proportional to the dialogs width.
void MempoolStats::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    ui->graphicsView->resize(size());
    ui->graphicsView->scene()->setSceneRect(rect());
    drawChart();
}

MempoolStats::~MempoolStats()
{

}