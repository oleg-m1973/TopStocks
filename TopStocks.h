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

	constexpr TChangePercent UpdateLastParice(const TPrice &price) noexcept
	{
		m_last = price;
		return TChangePercent((100.0 * (price - m_open) / m_open) * 100.0 + (price < m_open? -0.5: 0.5));
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

	auto SetUpdateTopsCallback(std::function<void (const CTopStocks &, bool, bool)> fn)
	{
		return std::exchange(m_fn, std::move(fn));
	}

	void OnQuote(const TStockID &id, const TPrice &price)
	{
		auto &stock = GetStock(id, price);
		if (stock.m_last == price)
			return;

		const TChangePercent change = stock.UpdateLastParice(price);
		if (stock.m_change == change)
			return;

		bool change_gainers = false;
		bool change_losers = false;
		
		TTopStocksMap::node_type node;
		if (stock.m_it)
		{
			change_gainers = stock.m_change >= m_gainers;
			change_losers = stock.m_change <= m_losers;

			node = m_tops.extract(*stock.m_it);
			stock.m_it = std::nullopt;
		}

		stock.m_change = change;
		if (change != 0)
		{
			if (!node)
				stock.m_it = m_tops.emplace(change, &stock);
			else
			{
				node.key() = change;
				stock.m_it = m_tops.insert(std::move(node));
			}
		}

		const auto update = UpdateTops(change);
		if (m_fn && (update.first || update.second || change_gainers || change_losers))
			m_fn(*this, update.first || change_gainers, update.second || change_losers);
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

	size_t GetStockCount() const noexcept
	{
		return m_stocks.size();
	}

	std::vector<CStock *> GetStocks() const
	{
		std::vector<CStock *> res;
		res.reserve(m_stocks.size());
		for (auto &item: m_stocks)
			res.emplace_back(item.second.get());
				
		return res;
	}

	constexpr size_t GetDepth() const noexcept
	{
		return m_depth;
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

	std::pair<bool, bool> UpdateTops(const TChangePercent &change) noexcept
	{
		if (m_tops.size() <= m_depth)
			return {true, true};

		bool gainers_changed = false;
		if (m_gainers == 0 || change >= m_gainers)
		{
			auto it = m_tops.rbegin();
			for (size_t i = 0; i < m_depth; ++i)
				++it;

			m_gainers = it->second->m_change;
			gainers_changed = true;
		}

		bool losers_changed = false;
		if (m_losers == 0 || change <= m_losers)
		{
			auto it = m_tops.begin();
			for (size_t i = 0; i < m_depth; ++i)
				++it;

			m_losers = it->second->m_change;
			losers_changed = true;
		}
		return {gainers_changed, losers_changed};
	}

	const size_t m_depth;
	std::unordered_map<TStockID, std::unique_ptr<CStock>> m_stocks;

	TChangePercent m_gainers = 0;
	TChangePercent m_losers = 0;
	TTopStocksMap m_tops;

	std::function<void (const CTopStocks &, bool, bool)> m_fn;
};
