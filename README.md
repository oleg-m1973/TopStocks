# TopStocks

Introduction
Implement a class that notifies a customer of top gainers/top losers on the stock market.The class will
get prices for different stocks. Your task is to calculate price changes for stocks considering first received
price of each stock as base. User of your class is interested only in top stocks 10
stocks with biggest
price changes and 10 stocks with smallest price changes. Every time when a newly arrived price affects
on one of these lists you should raise an appropriate notification with updated data.
Requirements
● Each price you get is a pair of stock id (integer) and price (double). Your class must have
OnQuote(int stock_id, double price) method which will be called every time when a new price
arrives.
● Consider first received price for each stock as base. For example, you have got the following
prices for one stock: 100.0 102.5 101.4 99.8 …
100.0 is a base price for the current session, you can calculate price changes since the second
price +2.5% +1.4% 0.2%
…
● Raise a notification about changes in the top lists every time when new stocks come to the top or
current order is changed.
● There should be 2 different notification top
gainers changed and top losers changed, each of
them should provide updated lists of stocks in correct order.
● Amount of different stocks can be up to 10000.
● Consider performance amount
of prices that you can process per second is critical.
