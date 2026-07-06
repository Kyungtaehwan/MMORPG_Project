#pragma once
#include <vector>
#include <mutex>
#include <cstring>
#include <cstdint>
#include "Protocol.h"

// ================================================================
//  CAuction_Manager  경매장(즉시구매) 전역 인메모리 매니저
//   - DB 없음: 서버 시작 시 기본 매물 하드코딩, 재시작하면 초기화.
//     실행 중 등록/구매/수령은 유지(하드코딩 인벤과 동일 취급).
//   - 아이템은 (코드, 개수) 모델. 인스턴스ID 없음(강화 미사용).
//   - 판매자 식별 = 계정 이름(재접속해도 유지).
//   - 개당 가격 + 부분 구매. 판매 골드는 매물에 누적되어 seller가 수령.
// ================================================================
struct FAuctionListing
{
    int32_t listingID;
    int32_t itemCode;
    int32_t count;        // 남은 수량
    int32_t unitPrice;    // 개당 가격
    int32_t pendingGold;  // 판매되어 미수령한 골드
    char    sellerName[20];
};

class CAuction_Manager
{
private:
    CAuction_Manager() { Init_Default(); }

public:
    static CAuction_Manager* Get_Instance()
    {
        static CAuction_Manager s;
        return &s;
    }

    // 현재 매물 전체를 스냅샷 패킷에 채운다.
    void Fill_Snapshot(SC_AUCTION_LIST_PACKET& pkt)
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        int32_t n = 0;
        for (const FAuctionListing& L : m_list)
        {
            if (n >= AUCTION_MAX) break;
            FAuctionEntry& e = pkt.entries[n];
            e.listingID = L.listingID;
            e.itemCode = L.itemCode;
            e.count = L.count;
            e.unitPrice = L.unitPrice;
            e.pendingGold = L.pendingGold;
            memcpy(e.sellerName, L.sellerName, sizeof(e.sellerName));
            ++n;
        }
        pkt.count = n;
    }

    // 등록. 성공 시 true.
    bool Register(const char* seller, int32_t itemCode, int32_t count, int32_t unitPrice)
    {
        if (itemCode <= 0 || count <= 0 || unitPrice <= 0) return false;
        std::lock_guard<std::mutex> lk(m_mtx);
        if (static_cast<int32_t>(m_list.size()) >= AUCTION_MAX) return false;

        FAuctionListing L{};
        L.listingID = m_nextID++;
        L.itemCode = itemCode;
        L.count = count;
        L.unitPrice = unitPrice;
        L.pendingGold = 0;
        strncpy_s(L.sellerName, seller ? seller : "", sizeof(L.sellerName) - 1);
        m_list.push_back(L);
        return true;
    }

    // 구매 가능 여부 확인(비변경). 성공 시 outItemCode/outTotal.
    //  - 본인 매물 불가, qty는 남은수량 이하.
    bool Peek_Buy(int32_t listingID, int32_t qty, const char* buyerName,
        int32_t& outItemCode, int32_t& outTotal)
    {
        if (qty <= 0) return false;
        std::lock_guard<std::mutex> lk(m_mtx);
        for (FAuctionListing& L : m_list)
        {
            if (L.listingID != listingID) continue;
            if (L.count < qty) return false;
            if (buyerName && strcmp(L.sellerName, buyerName) == 0) return false;
            outItemCode = L.itemCode;
            outTotal = L.unitPrice * qty;
            return true;
        }
        return false;
    }

    // 구매 확정(남은수량 차감 + 판매골드 누적). 성공 시 true.
    //  호출측이 Peek_Buy로 검증 후 골드/인벤 처리에 성공했을 때만 호출.
    bool Commit_Buy(int32_t listingID, int32_t qty, const char* buyerName)
    {
        if (qty <= 0) return false;
        std::lock_guard<std::mutex> lk(m_mtx);
        for (FAuctionListing& L : m_list)
        {
            if (L.listingID != listingID) continue;
            if (L.count < qty) return false;
            if (buyerName && strcmp(L.sellerName, buyerName) == 0) return false;
            L.count -= qty;
            L.pendingGold += L.unitPrice * qty;
            return true;
        }
        return false;
    }

    // 판매대금 수령. seller 본인 매물의 미수령 골드 회수. 성공 시 outGold.
    bool Collect(int32_t listingID, const char* seller, int32_t& outGold)
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        for (size_t i = 0; i < m_list.size(); ++i)
        {
            FAuctionListing& L = m_list[i];
            if (L.listingID != listingID) continue;
            if (!seller || strcmp(L.sellerName, seller) != 0) return false;
            if (L.pendingGold <= 0) return false;

            outGold = L.pendingGold;
            L.pendingGold = 0;
            if (L.count <= 0)   // 전량 판매 + 수령 완료 → 매물 제거
                m_list.erase(m_list.begin() + i);
            return true;
        }
        return false;
    }

    // 취소 정보 조회(비변경). seller 본인만. 남은수량/미수령골드/코드 반환.
    bool Peek_Cancel(int32_t listingID, const char* seller,
        int32_t& outCode, int32_t& outCount, int32_t& outGold)
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        for (FAuctionListing& L : m_list)
        {
            if (L.listingID != listingID) continue;
            if (!seller || strcmp(L.sellerName, seller) != 0) return false;
            outCode = L.itemCode;
            outCount = L.count;
            outGold = L.pendingGold;
            return true;
        }
        return false;
    }

    // 매물 제거(취소 확정). seller 본인만.
    bool Remove_Listing(int32_t listingID, const char* seller)
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        for (size_t i = 0; i < m_list.size(); ++i)
        {
            if (m_list[i].listingID != listingID) continue;
            if (!seller || strcmp(m_list[i].sellerName, seller) != 0) return false;
            m_list.erase(m_list.begin() + i);
            return true;
        }
        return false;
    }

private:
    // 서버 시작 시 기본 매물(판매자 "경매장"). 재시작 시 초기화됨.
    void Init_Default()
    {
        auto Add = [&](int32_t code, int32_t cnt, int32_t price)
        {
            FAuctionListing L{};
            L.listingID = m_nextID++;
            L.itemCode = code;
            L.count = cnt;
            L.unitPrice = price;
            L.pendingGold = 0;
            strncpy_s(L.sellerName, "경매장", sizeof(L.sellerName) - 1);
            m_list.push_back(L);
        };

        Add(1000, 20, 18);   // HP 물약(중)
        Add(1001, 10, 30);   // HP 물약(대)
        Add(1002, 20, 18);   // MP 물약(중)
        Add(1005, 2, 150);   // 무적 물약
        Add(3001, 1, 400);   // 소드
        Add(3007, 1, 250);   // 방어구
        Add(3022, 1, 200);   // 방패
    }

    std::vector<FAuctionListing> m_list;
    std::mutex m_mtx;
    int32_t m_nextID = 1;
};
