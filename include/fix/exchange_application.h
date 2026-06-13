#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <set>

#include <quickfix/Application.h>
#include <quickfix/MessageCracker.h>
#include <quickfix/SessionID.h>
#include <quickfix/fix42/NewOrderSingle.h>
#include <quickfix/fix42/OrderCancelRequest.h>
#include <quickfix/fix42/OrderCancelReplaceRequest.h>
#include <quickfix/fix42/ExecutionReport.h>
#include <quickfix/fix42/OrderCancelReject.h>

#include "exchange/exchange.h"
#include "risk/risk_context.h"
#include "common/types.h"

// ExecType values (FIX 4.2 tag 150)
namespace ExecType {
    constexpr char New          = '0';
    constexpr char PartialFill  = '1';
    constexpr char Fill         = '2';
    constexpr char Cancelled    = '4';
    constexpr char Replace      = '5';
    constexpr char Rejected     = '8';
}

// OrdStatus values (FIX 4.2 tag 39)
namespace OrdStatus {
    constexpr char New          = '0';
    constexpr char PartialFill  = '1';
    constexpr char Fill         = '2';
    constexpr char Cancelled    = '4';
    constexpr char Rejected     = '8';
}

// ─────────────────────────────────────────────────────────────────────────────
// ExchangeApplication
//
// Inherits FIX::Application  → receives QuickFIX session lifecycle events.
// Inherits FIX::MessageCracker → fromApp() is auto-dispatched to typed
//                                 onMessage() overloads.
// ─────────────────────────────────────────────────────────────────────────────
class ExchangeApplication
    : public FIX::Application
    , public FIX::MessageCracker
{
public:
    ExchangeApplication(Exchange& exchange, const RiskContext& risk_ctx);

    // ── FIX::Application session callbacks ───────────────────────────────────
    void onCreate (const FIX::SessionID&) override {}
    void onLogon  (const FIX::SessionID& session_id) override;
    void onLogout (const FIX::SessionID& session_id) override;
    void toAdmin  (FIX::Message& msg, const FIX::SessionID& session_id) override;
    void fromAdmin(const FIX::Message& msg, const FIX::SessionID& session_id) override;
    void toApp    (FIX::Message&, const FIX::SessionID&) override {}
    void fromApp  (const FIX::Message& msg, const FIX::SessionID& session_id) override;

    // ── FIX::MessageCracker typed handlers ───────────────────────────────────
    void onMessage(const FIX42::NewOrderSingle&,           const FIX::SessionID&);
    void onMessage(const FIX42::OrderCancelRequest&,        const FIX::SessionID&);
    void onMessage(const FIX42::OrderCancelReplaceRequest&, const FIX::SessionID&);

private:
    // ── Outbound message builders ─────────────────────────────────────────────
    void send_exec_report(const FIX::SessionID&   session_id,
                          const std::string&       cl_ord_id,
                          OrderId                  order_id,
                          const OrderState&        state,
                          char                     exec_type,
                          const std::string&       text = "");

    void send_exec_report(const FIX::SessionID&   session_id,
                          const std::string&       cl_ord_id,
                          OrderId                  order_id,
                          const OrderState&        state,
                          char                     exec_type,
                          Quantity                 last_shares,
                          Price                    last_px);

    void send_cancel_reject(const FIX::SessionID& session_id,
                            const std::string&     cl_ord_id,
                            const std::string&     orig_cl_ord_id,
                            const std::string&     reason);

    // ── Helpers ───────────────────────────────────────────────────────────────
    char order_status_char(OrderStatus s) const;
    std::shared_ptr<std::mutex> get_mutex_for_symbol(const std::string& symbol);

    // ── State ─────────────────────────────────────────────────────────────────
    Exchange&          exchange_;
    const RiskContext& risk_ctx_;

    std::mutex         global_state_mutex_; // Guards maps and ID allocators
    uint64_t           next_order_id_{1};
    uint64_t           next_exec_id_{1};

    // Maps client ClOrdID string -> internal OrderId
    std::unordered_map<std::string, OrderId> clordid_map_;
    // Maps internal OrderId -> client ClOrdID string
    std::unordered_map<OrderId, std::string> orderid_to_clordid_map_;

    // Maps order ID -> FIX SessionID for routing trade fills
    std::unordered_map<OrderId, FIX::SessionID> order_sessions_;

    // Mutex map for dynamic per-symbol locking
    std::mutex mutex_map_guard_;
    std::unordered_map<std::string, std::shared_ptr<std::mutex>> symbol_mutexes_;

    // Set of currently active logged-on client session IDs
    std::set<FIX::SessionID> active_sessions_;
};
