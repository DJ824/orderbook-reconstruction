import pandas as pd
import plotly.graph_objects as go
from plotly.subplots import make_subplots

df = pd.read_csv('imbalance_strat_log.csv', parse_dates=['timestamp'])

fig = make_subplots(rows=4, cols=1,
                    shared_xaxes=True,
                    vertical_spacing=0.02,
                    subplot_titles=("Price and Volume", "Strategy Position and P&L",
                                    "Order Book Imbalance", "Trade Executions"))

fig.add_trace(go.Scatter(x=df['timestamp'], y=df['mid_price'], name='Mid Price'),
              row=1, col=1)
fig.add_trace(go.Bar(x=df['timestamp'], y=df['volume'], name='Volume', yaxis='y2'),
              row=1, col=1)

fig.add_trace(go.Scatter(x=df['timestamp'], y=df['position'], name='Position'),
              row=2, col=1)
fig.add_trace(go.Scatter(x=df['timestamp'], y=df['pnl'], name='P&L', yaxis='y2'),
              row=2, col=1)

fig.add_trace(go.Scatter(x=df['timestamp'], y=df['imbalance'], name='Imbalance'),
              row=3, col=1)

buys = df[df['trade_direction'] == 1]
sells = df[df['trade_direction'] == -1]
fig.add_trace(go.Scatter(x=buys['timestamp'], y=buys['trade_price'],
                         mode='markers', name='Buy', marker=dict(color='green', symbol='triangle-up')),
              row=4, col=1)
fig.add_trace(go.Scatter(x=sells['timestamp'], y=sells['trade_price'],
                         mode='markers', name='Sell', marker=dict(color='red', symbol='triangle-down')),
              row=4, col=1)

fig.update_xaxes(title_text="Time", row=4, col=1)
fig.update_yaxes(title_text="Price", row=1, col=1)
fig.update_yaxes(title_text="Volume", overlaying='y', side='right', row=1, col=1)
fig.update_yaxes(title_text="Position", row=2, col=1)
fig.update_yaxes(title_text="P&L", overlaying='y', side='right', row=2, col=1)
fig.update_yaxes(title_text="Imbalance", row=3, col=1)
fig.update_yaxes(title_text="Price", row=4, col=1)

fig.update_layout(height=1200, width=1000, title_text="Imbalance Strategy Performance")

fig.show()

fig.write_html("strategy_visualization.html")