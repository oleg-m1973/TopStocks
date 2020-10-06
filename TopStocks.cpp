#define NOMINMAX

#include "TopStocks.h"

#include <Windows.h>
#include <iostream>
#include <random>
#include <chrono>
#include <iomanip>

class CTimer
{
public:
	using TClock = std::chrono::high_resolution_clock;

	template <typename T>
	constexpr void Diff(T &dt, bool fix = true) noexcept
	{
		const auto tm = TClock::now();
		dt = std::chrono::duration_cast<T>(tm - m_tm);

		if (fix)
			m_tm = tm;
	}

	template <typename T = std::chrono::milliseconds>
	constexpr T Diff(bool fix = true) noexcept
	{
		T dt;
		Diff(dt, fix);
		return dt;
	}

	constexpr const TClock::time_point &GetTime() const noexcept
	{
		return m_tm;
	}

	void Reset() noexcept
	{
		m_tm = TClock::now();
	}

	template <typename T = std::chrono::milliseconds, typename TFunc, typename... TT>
	static auto Measure(TFunc &&func, TT&&... args) noexcept(std::is_nothrow_invocable_v<TFunc, TT...>)
	{
		using TRes = std::invoke_result_t<TFunc, TT...>;
		const auto tm = TClock::now();
		if constexpr(std::is_same_v<TRes, void>)
		{
			std::invoke(std::forward<TFunc>(func), std::forward<TT>(args)...);
			return std::chrono::duration_cast<T>(TClock::now() - tm);
		}
		else 
		{
			const auto res = std::invoke(std::forward<TFunc>(func), std::forward<TT>(args)...);
			return std::make_pair(std::chrono::duration_cast<T>(TClock::now() - tm), res);
		}
	}
protected:
	TClock::time_point m_tm{TClock::now()};
};

static std::random_device _rd;
static std::mt19937 _gen(_rd());

static const TStockID StockNumber = 10000;

static std::uniform_int_distribution<TStockID> _rnd_stock_id(1, StockNumber);
static std::uniform_real_distribution<double> _rnd_stock_change(-20, 20);

static
std::pair<TStockID, TPrice> GenRandomStock()
{
	const auto id = _rnd_stock_id(_gen);
	const TPrice price = TPrice(id) + TPrice(id) * _rnd_stock_change(_gen) / 100.0;
	return {id, price};
}

void PrintStock(CStock &stock)
{
	printf("%6llu %9.2f %9.2f %6.2f%%", stock.m_id, stock.m_open, stock.m_last, double(stock.m_change) / 100.0);
}

void PrintTops(const CTopStocks &tops, bool update_gainers, bool update_loosers)
{

	auto h = ::GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO sbi = {};
	::GetConsoleScreenBufferInfo(h, &sbi);
	const SHORT c2 = 40;

	auto y = sbi.dwCursorPosition.Y;
	::SetConsoleCursorPosition(h, {0, y});
	printf(update_gainers? "Gainers*": "Gainers");

	::SetConsoleCursorPosition(h, {c2, y});
	printf(update_loosers? "Losers*": "Losers");
	++y;

	const auto yy = y;
	for (auto &item: tops.GetGainers())
	{
		::SetConsoleCursorPosition(h, {0, y});
		PrintStock(*item);
		++y;
	}

	y = yy;
	for (auto &item: tops.GetLosers())
	{
		::SetConsoleCursorPosition(h, {c2, y});
		PrintStock(*item);
		++y;
	}

	std::cout << std::endl;

	std::cout << "Stocks: " << tops.GetStockCount() << std::endl;
}

static 
void UnitTest()
{
	static const size_t _n = 1'000'000;
	CTopStocks tops;
	
	CTimer tm;

	for (size_t i = 0; i < _n; ++i)
	{
		const auto stock = GenRandomStock();
		tops.OnQuote(stock.first, stock.second);
		if (i == 0 || i % 10000 != 0)
			continue;

		auto stocks = tops.GetStocks();
		{
			auto it = std::remove_if(stocks.begin(), stocks.end(), [](auto *v)
			{
				return v->m_change == 0;
			});
			stocks.erase(it, stocks.end());
			std::sort(stocks.begin(), stocks.end(), [](auto *v1, auto *v2)
			{
				return v1->m_change < v2->m_change;
			});
		}

		const size_t n = std::min(stocks.size(), tops.GetDepth());
		size_t j = 0;
		{
				
			auto it = stocks.begin();
			for (auto &item: tops.GetLosers())
			{
				if ((*it++)->m_change != item->m_change)
					throw std::runtime_error("LOSERS!!!!!!!!!!!!!!!!!!");
				++j;
			}
		}
		if (j != n)
			throw std::runtime_error("LOSERS!!!!!!!!!!!!!!!!!!");

		j = 0;
		{
			auto it = stocks.rbegin();
			for (auto &item: tops.GetGainers())
			{
				if ((*it++)->m_change != item->m_change)
					throw std::runtime_error("GAINERS!!!!!!!!!!!!!!!!!!");
				++j;
			}
		}
		if (j != n)
			throw std::runtime_error("LOSERS!!!!!!!!!!!!!!!!!!");
		
		system("cls");
		std::cout << (_n - i) << std::endl;

		PrintTops(tops, true, true);
	}

	const auto dt = tm.Diff();
	std::cout << "OK: " << dt.count() << "ms" << std::endl;
}

void PerformanceTest()
{
	static const size_t _n = 1'000'000;
	CTopStocks tops;
	
	CTimer tm;

	for (size_t i = 0; i < _n; ++i)
	{
		const auto stock = GenRandomStock();
		tops.OnQuote(stock.first, stock.second);
	}
}

int main()
{
	try
	{
		std::pair<TStockID, TPrice> stocks[] = 
		{
			{34, 634.12},
			{34, 697.20},
			{235, 23.87},
			{235, 25.60},
			{9722, 53.10},
			{9722, 56.90},
			{482, 0.54},
			{482, 0.55},

			{523, 324.90},
			{523, 287.2},
			{1093, 83.55},
			{1093, 76.5},
			{618, 1.35},
			{618, 1.28},
			{208, 5.66},
			{208, 5.55},
		};

		//CTopStocks tops;
		//tops.SetUpdateTopsCallback([](auto &tops, bool update_gainers, bool update_loosers)
		//{
		//	system("cls");
		//	PrintTops(tops, update_gainers, update_loosers);
		//});

		//for (auto &item: stocks)
		//{
		//	tops.OnQuote(item.first, item.second);
		//}

		UnitTest();
		//PrintAt(20, 0, "!!!: ", 123445);

	}
	catch (const std::exception &e)
	{
		std::cerr << "ERROR: " << e.what() << std::endl;
	}
}
