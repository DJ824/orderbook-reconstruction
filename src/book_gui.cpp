#include "book_gui.h"
#include <QtCharts/QDateTimeAxis>
#include <QMargins>
#include <QGraphicsScene>
#include <QGraphicsProxyWidget>
#include <QDateTime>
#include <algorithm>
#include <QVBoxLayout>
#include <limits>
#include <tuple>
#include <QWheelEvent>

BookGui::BookGui(QWidget *parent)
        : QWidget(parent), m_bid_series_(new QLineSeries(this)), m_ask_series_(new QLineSeries(this)),
          m_chart(new QChart()), m_chart_view_(new QChartView(m_chart, this)), m_axis_x_(new QDateTimeAxis()),
          m_axis_y_(new QValueAxis()), m_update_timer_(new QTimer(this)),
          m_horizontal_scroll_bar_(new QScrollBar(Qt::Horizontal, this)), m_start_button_(new QPushButton("Start", this)),
          m_stop_button_(new QPushButton("Stop", this)), m_progress_bar_(new QProgressBar(this)), m_auto_scroll_(true),
          m_user_scrolling_(false) {
    setup_chart();
    setup_scroll_bar();
    setup_buttons();

    connect(m_update_timer_, &QTimer::timeout, this, &BookGui::update_chart);
    m_update_timer_->start(UPDATE_INTERVAL_);
}

void BookGui::setup_chart() {
    m_chart->addSeries(m_bid_series_);
    m_chart->addSeries(m_ask_series_);

    m_chart->setTitle("HFT Price Chart");
    m_chart->legend()->setVisible(true);
    m_chart->legend()->setAlignment(Qt::AlignBottom);

    m_bid_series_->setName("Best Bid");
    m_ask_series_->setName("Best Ask");

    m_bid_series_->setColor(QColor(4, 165, 229));  // RGB color for best bid
    m_ask_series_->setColor(QColor(136, 57, 239)); // RGB color for best ask

    m_chart->addAxis(m_axis_x_, Qt::AlignBottom);
    m_chart->addAxis(m_axis_y_, Qt::AlignRight);

    m_bid_series_->attachAxis(m_axis_x_);
    m_bid_series_->attachAxis(m_axis_y_);
    m_ask_series_->attachAxis(m_axis_x_);
    m_ask_series_->attachAxis(m_axis_y_);

    m_axis_x_->setFormat("hh:mm:ss");
    m_axis_x_->setTickCount(6);
    m_axis_y_->setLabelFormat("%i");
    m_axis_y_->setTickCount(NUM_PRICE_TICKS_);

    style_axis_labels(m_axis_x_);
    style_axis_labels(m_axis_y_);

    m_chart_view_->setChart(m_chart);
    m_chart_view_->setRenderHint(QPainter::Antialiasing);

    m_chart->setBackgroundBrush(QBrush(Qt::black));
    m_chart->setPlotAreaBackgroundBrush(QBrush(Qt::black));
    m_chart->setPlotAreaBackgroundVisible(true);
}

void BookGui::setup_scroll_bar() {
    m_horizontal_scroll_bar_->setOrientation(Qt::Horizontal);
    m_horizontal_scroll_bar_->setFixedHeight(15);
    connect(m_horizontal_scroll_bar_, &QScrollBar::valueChanged, this, &BookGui::handle_scroll_bar_value_changes);
    connect(m_horizontal_scroll_bar_, &QScrollBar::sliderPressed, [this]() {
        m_auto_scroll_ = false;
        m_user_scrolling_ = true;
    });
    connect(m_horizontal_scroll_bar_, &QScrollBar::sliderReleased, [this]() {
        m_user_scrolling_ = false;
    });
}

void BookGui::setup_buttons() {
    m_stop_button_->setEnabled(false);

    connect(m_start_button_, &QPushButton::clicked, this, &BookGui::handle_start_button_click);
    connect(m_stop_button_, &QPushButton::clicked, this, &BookGui::handle_stop_button_click);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(m_start_button_);
    buttonLayout->addWidget(m_stop_button_);

    m_progress_bar_->setRange(0, 100);
    m_progress_bar_->setValue(0);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(buttonLayout);
    mainLayout->addWidget(m_chart_view_);
    mainLayout->addWidget(m_horizontal_scroll_bar_);
    mainLayout->addWidget(m_progress_bar_);

    setLayout(mainLayout);
}

void BookGui::add_data_point(qint64 timestamp, double bestBid, double bestAsk) {
    m_data_points_.emplace_back(timestamp, bestBid, bestAsk);

    if (m_data_points_.size() > MAX_POINTS_) {
        m_data_points_.pop_front();
    }

    update_chart();
}

void BookGui::update_chart() {
    if (m_data_points_.empty()) return;

    m_bid_series_->clear();
    m_ask_series_->clear();

    qint64 latestTimestamp = std::get<0>(m_data_points_.back());
    qint64 oldestTimestamp = std::get<0>(m_data_points_.front());

    double minPrice = std::numeric_limits<double>::max();
    double maxPrice = std::numeric_limits<double>::lowest();

    for (const auto &point: m_data_points_) {
        qint64 timestamp = std::get<0>(point);
        double bestBid = std::get<1>(point);
        double bestAsk = std::get<2>(point);

        m_bid_series_->append(timestamp, bestBid);
        m_ask_series_->append(timestamp, bestAsk);

        minPrice = std::min({minPrice, bestBid, bestAsk});
        maxPrice = std::max({maxPrice, bestBid, bestAsk});
    }

    int scrollBarRange = latestTimestamp - oldestTimestamp - VISIBLE_TIME_RANGE_;
    m_horizontal_scroll_bar_->setRange(0, scrollBarRange > 0 ? scrollBarRange : 0);
    m_horizontal_scroll_bar_->setPageStep(VISIBLE_TIME_RANGE_);

    if (m_auto_scroll_ && !m_user_scrolling_) {
        m_horizontal_scroll_bar_->setValue(scrollBarRange);
        qint64 visibleStartTimestamp = latestTimestamp - VISIBLE_TIME_RANGE_;
        m_axis_x_->setRange(QDateTime::fromMSecsSinceEpoch(visibleStartTimestamp),
                            QDateTime::fromMSecsSinceEpoch(latestTimestamp));
    } else if (!m_user_scrolling_) {
        // Update the visible range based on the current scroll position
        qint64 visibleStartTimestamp = oldestTimestamp + m_horizontal_scroll_bar_->value();
        m_axis_x_->setRange(QDateTime::fromMSecsSinceEpoch(visibleStartTimestamp),
                            QDateTime::fromMSecsSinceEpoch(visibleStartTimestamp + VISIBLE_TIME_RANGE_));
    }

    update_axis_y(minPrice, maxPrice);
}

void BookGui::update_axis_y(double minPrice, double maxPrice) {
    int lowerBound = static_cast<int>(minPrice / PRICE_INTERVAL_) * PRICE_INTERVAL_;
    int upperBound = (static_cast<int>(maxPrice / PRICE_INTERVAL_) + 1) * PRICE_INTERVAL_;

    while ((upperBound - lowerBound) / PRICE_INTERVAL_ < NUM_PRICE_TICKS_) {
        lowerBound -= PRICE_INTERVAL_;
        upperBound += PRICE_INTERVAL_;
    }

    m_axis_y_->setRange(lowerBound, upperBound);
    m_axis_y_->setTickCount((upperBound - lowerBound) / PRICE_INTERVAL_ + 1);
}

void BookGui::style_axis_labels(QAbstractAxis *axis) {
    QFont font = axis->labelsFont();
    font.setPointSize(10);
    axis->setLabelsFont(font);
    axis->setLabelsColor(Qt::white);
    axis->setGridLineColor(QColor(100, 100, 100));
    axis->setGridLineVisible(true);

    QPen axisPen(Qt::white);
    axisPen.setWidth(2);
    axis->setLinePen(axisPen);
}

void BookGui::handle_scroll_bar_value_changes(int value) {
    if (m_data_points_.empty() || m_auto_scroll_) return;

    qint64 oldestTimestamp = std::get<0>(m_data_points_.front());
    qint64 visibleStartTimestamp = oldestTimestamp + value;

    m_axis_x_->setRange(QDateTime::fromMSecsSinceEpoch(visibleStartTimestamp),
                        QDateTime::fromMSecsSinceEpoch(visibleStartTimestamp + VISIBLE_TIME_RANGE_));
}

void BookGui::handle_start_button_click() {
    m_start_button_->setEnabled(false);
    m_stop_button_->setEnabled(true);
    m_auto_scroll_ = true;
    emit start_backtest();
}

void BookGui::handle_stop_button_click() {
    m_start_button_->setEnabled(true);
    m_stop_button_->setEnabled(false);
    emit stop_backtest();
}

void BookGui::update_progress(int progress) {
    m_progress_bar_->setValue(progress);
}

void BookGui::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    m_horizontal_scroll_bar_->setGeometry(0, height() - 15, width(), 15);
}

void BookGui::wheelEvent(QWheelEvent *event) {
    QWidget::wheelEvent(event);
}