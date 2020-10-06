#pragma once
#include <unordered_map>
#include <map>
#include <optional>
#include <memory>
#include <functional>

class CStock;
class CTopStocks;

using TStockID = uintptr_t;
using TPrice = double;
using TChangePercent = intptr_t; 
using TTopStocksMap = std::multimap<TChangePercent, CStock *>;

class CStock
{
friend CTopStocks;
public:
	CStock(TStockID id, TPrice price)
	: m_id(id)
	, m_open(price)
	, m_last(price)
	{
	}

	const TStockID m_id;
	const TPrice m_open = 0;
	TPrice m_last = 0;

	TChangePercent m_change = 0;
private:
	std::optional<TTopStocksMap::iterator> m_it;
};

class CTopStocks
{
public:
	CTopStocks(size_t depth = 10)
	: m_depth(depth)
	{
	}

	auto SetUpdateTopsCallback(std::function<void (const CTopStocks &)> fn)
	{
		return std::exchange(m_fn, std::move(fn));
	}

	void OnQuote(const TStockID &id, const TPrice &price)
	{
		auto &stock = GetStock(id, price);
		if (stock.m_last == price)
			return;

		stock.m_last = price;
		const TChangePercent change = TChangePercent((100.0 * (price - stock.m_open) / stock.m_open) * 100);
		if (stock.m_change == change)
			return;

		stock.m_change = change;
		if (stock.m_it)
		{
			m_tops.erase(*stock.m_it);
			stock.m_it = std::nullopt;
		}

		if (change != 0)
			stock.m_it = m_tops.emplace(change, &stock);

		if (UpdateTops(change) && m_fn)
			m_fn(*this);
	}

	std::vector<CStock *> GetGainers() const
	{
		return GetGainers(m_depth);
	}

	std::vector<CStock *> GetGainers(size_t depth) const
	{
		return GetTops(m_tops.rbegin(), m_tops.rend(), depth);
	}

	std::vector<CStock *> GetLosers() const
	{
		return GetLosers(m_depth);
	}

	std::vector<CStock *> GetLosers(size_t depth) const
	{
		return GetTops(m_tops.begin(), m_tops.end(), depth);
	}

protected:
	template <typename It>
	std::vector<CStock *> GetTops(It it, It end, size_t depth) const
	{
		std::vector<CStock *> res;
		res.reserve(depth);
		for (; it != end && depth; ++it, --depth)
			res.emplace_back(it->second);

		return res;
	}


	CStock &GetStock(const TStockID &id, const TPrice &price)
	{
		auto it = m_stocks.emplace(id, nullptr);
		if (it.second)
			try
			{
				it.first->second.reset(new CStock(id, price));
			}
			catch (...)
			{
				m_stocks.erase(it.first);
				throw;
			}

		return *it.first->second;
	}

	bool UpdateTops(const TChangePercent &change) noexcept
	{
		if (m_tops.size() <= m_depth)
			return true;

		bool tops_changed = false;
		if (m_gainers == 0 || change > m_gainers)
		{
			auto it = m_tops.rbegin();
			for (size_t i = 0; i < m_depth; ++i)
				++it;

			m_gainers = it->second->m_change;
			tops_changed = true;
		}

		if (m_losers == 0 || change < m_losers)
		{
			auto it = m_tops.begin();
			for (size_t i = 0; i < m_depth; ++i)
				++it;

			m_losers = it->second->m_change;
			tops_changed = true;
		}
		return tops_changed;
	}

	const size_t m_depth;
	std::unordered_map<TStockID, std::unique_ptr<CStock>> m_stocks;

	TChangePercent m_gainers = 0;
	TChangePercent m_losers = 0;
	TTopStocksMap m_tops;

	std::function<void (const CTopStocks &)> m_fn;
};
