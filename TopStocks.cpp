#include "TopStocks.h"

#include <iostream>

#include <random>

template <typename TItems>
void PrintTops(const char *name, TItems &&items)
{
	std::cout << name << std::endl;
	for (auto &item: items)
		std::cout << item->m_id << ": " << item->m_open << ", " << item->m_last << ", " << item->m_change << std::endl;

}

void PrintTops(const CTopStocks &tops)
{
	const auto gainers = tops.GetGainers();
	const auto losers = tops.GetLosers();

	system("cls");
	PrintTops("Gainers", gainers);
	PrintTops("Losers", losers);
	system("pause");
}

int main()
{
	//std::random_device rd;
	//std::mt19937 gen(rd());
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

	CTopStocks tops;
	tops.SetUpdateTopsCallback(PrintTops);
	for (auto &item: stocks)
	{
		tops.OnQuote(item.first, item.second);
	}
	
}
