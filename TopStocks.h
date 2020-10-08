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
	CStock(TStockID id)
	: m_id(id)
	{
	}

	bool UpdateLastPrice(const TPrice &price) noexcept
	{
		if (m_open == 0)
		{
			m_open = m_last = price;
			m_change = 0;
			return false;
		}
		m_last = price;
		m_change = TChangePercent((100.0 * (price - m_open) / m_open) * 100.0 + (price < m_open? -0.5: 0.5));
		return true;
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
	TPrice m_open = 0;
	TPrice m_last = 0;

	TChangePercent m_change = 0;
private:
	std::optional<TTopStocksMap::iterator> m_it;
};

class CTopStocks
{
public:
	CTopStocks(size_t depth = 10, TStockID max_quote_id = 10000)
	: m_depth(depth)
	{
		m_stocks.reserve(max_quote_id + 1);
		for (TStockID id = 0; id <= max_quote_id; ++id)
			m_stocks.emplace_back(std::make_unique<CStock>(id));
	}

	auto SetUpdateTopsCallback(std::function<void (const CTopStocks &, bool, bool)> fn)
	{
		return std::exchange(m_fn, std::move(fn));
	}

	void OnQuote(const TStockID &id, const TPrice &price)
	{
		auto &stock = GetStock(id);
		const TChangePercent change_prev = stock.m_change;

		if (!stock.UpdateLastPrice(price))
			return;

		if (change_prev == stock.m_change)
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

		const bool update_gainers = (change_prev >= m_gainers) || (stock.m_change >= m_gainers);
		const bool update_losers = (change_prev <= m_losers) || (stock.m_change <= m_losers);
		
		UpdateTops(update_gainers, update_losers);

		if (m_fn && (update_gainers || update_losers))
			m_fn(*this, update_gainers, update_losers);
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
			if (item)
				res.emplace_back(item.get());
				
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

	CStock &GetStock(const TStockID &id)
	{
		if (id >= m_stocks.size())
			m_stocks.resize(id + 1);

		auto &sp = m_stocks[id];
		return !sp? *(sp = std::make_unique<CStock>(id)): *sp;
	}

	void UpdateTops(bool update_gainers, bool update_losers) noexcept
	{
		if (update_gainers)
		{
			m_gainers = 0;
			auto it = m_tops.rbegin();
			for (size_t i = 0, n = std::min(m_depth, m_tops.size()); i < n && it->second->IsGainer(); ++it, ++i)
				m_gainers = it->second->m_change;
		}

		if (update_losers)
		{
			m_losers = 0;
			auto it = m_tops.begin();
			for (size_t i = 0, n = std::min(m_depth, m_tops.size()); i < n  && it->second->IsLoser(); ++it, ++i)
				m_losers = it->second->m_change;
		}
	}

	const size_t m_depth;
	std::vector<std::unique_ptr<CStock>> m_stocks;

	TChangePercent m_gainers = 0;
	TChangePercent m_losers = 0;
	TTopStocksMap m_tops;

	std::function<void (const CTopStocks &, bool, bool)> m_fn;
};
