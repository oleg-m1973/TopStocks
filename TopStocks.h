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
		const auto change = m_change;
		m_change = TChangePercent((100.0 * (price - m_open) / m_open) * 100.0 + (price < m_open? -0.5: 0.5));
		return change;
	}

	constexpr double GetChangPercent() const noexcept
	{
		return double(m_change) / 100.0;
	}

	constexpr bool IsGainer() const noexcept
	{
		return m_change > 0;
	}

	constexpr bool IsLoser() const noexcept
	{
		return m_change < 0;
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

		const TChangePercent change_prev = stock.UpdateLastParice(price);
		if (stock.m_change == change_prev)
			return;

		TTopStocksMap::node_type node;
		if (stock.m_it)
		{
			node = m_tops.extract(*stock.m_it);
			stock.m_it = std::nullopt;
		}

		if (stock.m_change != 0)
		{
			if (!node)
				stock.m_it = m_tops.emplace(stock.m_change, &stock);
			else
			{
				node.key() = stock.m_change;
				stock.m_it = m_tops.insert(std::move(node));
			}
		}

		const auto update = UpdateTops(change_prev, stock.m_change);
		if (m_fn && (update.first || update.second))
			m_fn(*this, update.first, update.second);
	}

	std::vector<const CStock *> GetGainers() const
	{
		return GetGainers(m_depth);
	}

	std::vector<const CStock *> GetGainers(size_t depth) const
	{
		return GetTops(m_tops.rbegin(), m_tops.rend(), depth, true);
	}

	std::vector<const CStock *> GetLosers() const
	{
		return GetLosers(m_depth);
	}

	std::vector<const CStock *> GetLosers(size_t depth) const
	{
		return GetTops(m_tops.begin(), m_tops.end(), depth, false);
	}

	size_t GetStockCount() const noexcept
	{
		return m_stocks.size();
	}

	std::vector<const CStock *> GetStocks() const
	{
		std::vector<const CStock *> res;
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
	auto GetTops(It it, It end, size_t depth, bool gainer) const
	{
		std::vector<const CStock *> res;
		res.reserve(depth);
		for (; it != end && depth && (gainer? it->second->IsGainer(): it->second->IsLoser()); ++it, --depth)
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

	std::pair<bool, bool> UpdateTops(const TChangePercent &change_prev, const TChangePercent &change) noexcept
	{
		if (m_tops.size() <= m_depth)
			return {true, true};

		bool gainers_changed = false;
		if (m_gainers == 0 || change >= m_gainers || change_prev >= m_gainers)
		{
			auto it = m_tops.rbegin();
			for (size_t i = 0; i < m_depth && it->second->IsGainer(); ++it, ++i)
				m_gainers = it->second->m_change;

			gainers_changed = true;
		}

		bool losers_changed = false;
		if (m_losers == 0 || (change != 0 && change <= m_losers) || (change_prev != 0 && change_prev <= m_losers))
		{
			auto it = m_tops.begin();
			for (size_t i = 0; i < m_depth && it->second->IsLoser(); ++it, ++i)
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
