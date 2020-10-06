#define NOMINMAX

#include "TopStocks.h"

#include <Windows.h>
#include <iostream>
#include <random>
#include <chrono>
#include <thread>
#include <fstream>
#include <conio.h>

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

static
void PrintStock(CStock &stock)
{
	printf("%6zu %9.2f %9.2f %6.2f%%", stock.m_id, stock.m_open, stock.m_last, stock.GetChangPercent());
}

static
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

	::SetConsoleCursorPosition(h, {0, yy + SHORT(tops.GetDepth())});
	std::cout << std::endl;

	std::cout << "Stocks: " << tops.GetStockCount() << std::endl;
}

static 
void UnitTest()
{
	std::cout << __FUNCTION__ << std::endl;
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
		{
			auto items = tops.GetLosers();
			for (size_t i = 0; i < n && stocks[i]->IsLoser(); ++i)
				if (i >= items.size() || items[i]->m_change != stocks[i]->m_change)
					throw std::runtime_error("LOSERS!!!!!!!!!!!!!!!!!!");
		}

		{
			auto items = tops.GetGainers();
			for (size_t i = 0, j = stocks.size() - 1; i < n && stocks[j]->IsGainer(); ++i, --j)
				if (i >= items.size() || items[i]->m_change != stocks[j]->m_change)
					throw std::runtime_error("GAINERS!!!!!!!!!!!!!!!!!!");
		}
		
		system("cls");
		std::cout << (_n - i) << std::endl;

		PrintTops(tops, true, true);
	}

	const auto dt = tm.Diff();
	std::cout << "OK: " << dt.count() <<  " ms" << std::endl;
}

static
void PerformanceTest()
{
	std::cout << __FUNCTION__ << std::endl;
	std::cout << "Test running...";
	static const size_t _n = 10'000'000;
	size_t update_gainers = 0;
	size_t update_losers = 0;
	CTopStocks tops;
	tops.SetUpdateTopsCallback([&](auto &, bool gnr, bool lsr)
	{
		if (gnr)
			++update_gainers;

		if (lsr)
			++update_losers;
	});


	CTimer tm;

	for (size_t i = 0; i < _n; ++i)
	{
		const auto stock = GenRandomStock();
		tops.OnQuote(stock.first, stock.second);
	}
	const auto dt = tm.Diff();

	system("cls");
	std::cout 
		<< "Trades     : " << _n << std::endl
		<< "Stocks     : " << tops.GetStockCount() << std::endl
		<< "Upd Gainers: " << update_gainers << std::endl
		<< "Upd Losers : " << update_losers << std::endl
		<< "Time       : " << dt.count() << " ms" << std::endl
		;
}

static 
void RandomTest()
{
	std::cout << __FUNCTION__ << std::endl;
	size_t update_gainers = 0;
	size_t update_losers = 0;
	bool delay = false;

	CTopStocks tops;
	tops.SetUpdateTopsCallback([&](auto &tops, bool gnr, bool lsr)
	{
		if (gnr)
			++update_gainers;

		if (lsr)
			++update_losers;

		system("cls");
		PrintTops(tops, gnr, lsr);
		std::cout << "Press any key to finish" << std::endl;
		delay = true;
	});

	for (size_t i = 0; !_kbhit(); ++i)
	{
		const auto stock = GenRandomStock();
		tops.OnQuote(stock.first, stock.second);

		if (delay)
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	_getch();
}

static 
void ReadFromFile()
{
	std::cout << __FUNCTION__ << std::endl;
	CTopStocks tops;
	tops.SetUpdateTopsCallback([&](auto &tops, bool gnr, bool lsr)
	{
		system("cls");
		PrintTops(tops, gnr, lsr);
	});

	std::ifstream in("stocks.txt");
	if (!in)
	{
		const auto err = ::GetLastError();
		std::cerr << "Can't open file, err: " << err << std::endl;
		return;
	}

	TStockID id;
	TPrice price;
	while (in >> id >> price)
		tops.OnQuote(id, price);

	system("cls");
	PrintTops(tops, true, true);
}
int main()
{
	//setlocale(LC_ALL, "Russian");
	try
	{
		for (;;)
		{
			system("cls");
			std::cout << "1: Unit test" << std::endl;
			std::cout << "2: Read from file ./stocks.txt" << std::endl;
			std::cout << "3: Random stocks" << std::endl;
			std::cout << "4: Benchmark" << std::endl;
			std::cout << "0: exit" << std::endl;
		
			int n = 0;
			std::cin >> n;

			system("cls");
			switch (n)
			{
			case 1: UnitTest(); break;
			case 2: ReadFromFile(); break;
			case 3: RandomTest(); break;
			case 4: PerformanceTest(); break; 
			case 0: return 0;
			default: continue;
			}
			system("pause");
		}
	}
	catch (const std::exception &e)
	{
		std::cerr << "ERROR: " << e.what() << std::endl;
	}
}
