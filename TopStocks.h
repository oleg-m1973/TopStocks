#pragma once
#include <unordered_map>
#include <map>
#include <optional>
#include <memory>
#include <functional>

class CStock;
class CTopStocks;
class CTopStocks2;

using TStockID = uintptr_t;
using TPrice = double;
using TChangePercent = intptr_t; 
using TTopStocksMap = std::multimap<TChangePercent, CStock *>;

class CStock
{
friend CTopStocks;
friend CTopStocks2;
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

	//CTopStocks2
	bool m_gainer = false;
	bool m_loser = false;
};

class CStocks
{
public:
	CStocks(TStockID max_quote_id)
	{
		m_stocks.reserve(max_quote_id + 1);
		for (TStockID id = 0; id <= max_quote_id; ++id)
			m_stocks.emplace_back(std::make_unique<CStock>(id));
	}

	CStock &GetStock(const TStockID &id)
	{
		if (id >= m_stocks.size())
			m_stocks.resize(id + 1);

		auto &sp = m_stocks[id];
		return !sp? *(sp = std::make_unique<CStock>(id)): *sp;
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
protected:
	std::vector<std::unique_ptr<CStock>> m_stocks;
};

class CTopStocks
{
public:
	CTopStocks(size_t depth = 10, TStockID max_quote_id = 10000)
	: m_depth(depth)
	, m_stocks(max_quote_id)
	{
	}

	auto SetUpdateTopsCallback(std::function<void (const CTopStocks &, bool, bool)> fn)
	{
		return std::exchange(m_fn, std::move(fn));
	}

	void OnQuote(const TStockID &id, const TPrice &price)
	{
		auto &stock = m_stocks.GetStock(id);
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

	std::vector<const CStock *> GetStocks() const
	{
		return m_stocks.GetStocks();
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
	CStocks m_stocks;

	TChangePercent m_gainers = 0;
	TChangePercent m_losers = 0;
	TTopStocksMap m_tops;

	std::function<void (const CTopStocks &, bool, bool)> m_fn;
};


//Alternative implementation
class CTopStocks2
{
public:
	using TCallback = std::function<void (const std::vector<const CStock *> &)>;

	CTopStocks2(size_t depth = 10, TStockID max_quote_id = 10000)
	: m_depth(depth)
	, m_stocks(max_quote_id)
	{
		m_gainers.reserve(m_depth);
		m_losers.reserve(m_depth);
	}

	auto SetGainersCallback(TCallback fn)
	{
		return std::exchange(m_fn_gainers, std::move(fn));
	}

	auto SetLosersCallback(TCallback fn)
	{
		return std::exchange(m_fn_losers, std::move(fn));
	}

	void OnQuote(const TStockID &id, const TPrice &price)
	{
		auto &stock = m_stocks.GetStock(id);
		const TChangePercent change_prev = stock.m_change;

		if (!stock.UpdateLastPrice(price))
			return;

		if (change_prev == stock.m_change)
			return;

		const bool update_gainers = stock.m_gainer;
		const bool update_losers = stock.m_loser;

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

		UpdateTops(stock);
		
		if (m_fn_gainers && (update_gainers || stock.m_gainer))
			m_fn_gainers(m_gainers);

		if (m_fn_losers && (update_losers || stock.m_loser))
			m_fn_losers(m_losers);
	}

	constexpr size_t GetDepth() const noexcept
	{
		return m_depth;
	}

protected:
	void UpdateTops(CStock &stock) noexcept
	{
		if (stock.m_gainer || m_gainers.empty() || stock.m_change >= m_gainers.back()->m_change)
		{
			stock.m_gainer = false;
			m_gainers.clear();
			auto it = m_tops.rbegin();
			for (size_t i = 0, n = std::min(m_depth, m_tops.size()); i < n; ++it, ++i)
			{
				auto &stock = *it->second;
				if (!stock.IsGainer())
					break;
				stock.m_gainer = true;
				m_gainers.emplace_back(&stock);
			}
		}

		if (stock.m_loser || m_losers.empty() || stock.m_change <= m_losers.back()->m_change)
		{
			stock.m_loser = false;
			m_losers.clear();
			auto it = m_tops.begin();
			for (size_t i = 0, n = std::min(m_depth, m_tops.size()); i < n; ++it, ++i)
			{
				auto &stock = *it->second;
				if (!stock.IsLoser())
					break;
				stock.m_loser = true;
				m_losers.emplace_back(&stock);
			}
		}
	}

	const size_t m_depth;
	CStocks m_stocks;

	TTopStocksMap m_tops;
	std::vector<const CStock *> m_gainers;
	std::vector<const CStock *> m_losers;

	TCallback m_fn_gainers;
	TCallback m_fn_losers;
};
