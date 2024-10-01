#pragma once

#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>
#include <QTimer>
#include <QScrollBar>
#include <QGraphicsView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProgressBar>
#include <deque>

class BookGui : public QWidget {
Q_OBJECT

public:
    explicit BookGui(QWidget *parent = nullptr);

    void add_data_point(qint64 timestamp, double bestBid, double bestAsk);

public slots:

    void update_progress(int progress);

signals:

    void start_backtest();

    void stop_backtest();

private:
    QLineSeries *m_bid_series_;
    QLineSeries *m_ask_series_;
    QChart *m_chart;
    QChartView *m_chart_view_;
    QDateTimeAxis *m_axis_x_;
    QValueAxis *m_axis_y_;
    QTimer *m_update_timer_;
    QScrollBar *m_horizontal_scroll_bar_;
    std::deque<std::tuple<qint64, double, double>> m_data_points_;
    QPushButton *m_start_button_;
    QPushButton *m_stop_button_;
    QProgressBar *m_progress_bar_;
    bool m_auto_scroll_;
    bool m_user_scrolling_;

    const int MAX_POINTS_ = 1000;
    const int UPDATE_INTERVAL_ = 100;
    const int PRICE_INTERVAL_ = 25;
    const int NUM_PRICE_TICKS_ = 10;
    const qint64 VISIBLE_TIME_RANGE_ = 300000;

    void setup_chart();
    void setup_scroll_bar();
    void setup_buttons();
    void update_chart();
    void update_axis_y(double minPrice, double maxPrice);
    void style_axis_labels(QAbstractAxis *axis);

private slots:

    void handle_scroll_bar_value_changes(int value);

    void handle_start_button_click();

    void handle_stop_button_click();

protected:
    void resizeEvent(QResizeEvent *event) override;

    void wheelEvent(QWheelEvent *event) override;
};