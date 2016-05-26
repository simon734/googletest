#ifndef TGP_ACCL_TGPCOMM_UDP_LOST_CALC_EX_H
#define TGP_ACCL_TGPCOMM_UDP_LOST_CALC_EX_H
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 基本数据类型定义
#ifdef WIN32
#include <windows.h>

typedef signed char       int8_t;
typedef short             int16_t;  // NOLINT
typedef int               int32_t;

typedef unsigned char     uint8_t;
typedef unsigned short    uint16_t;  // NOLINT
typedef unsigned int      uint32_t;

typedef __int64            int64_t;
typedef unsigned __int64   uint64_t;

#else
#include <stdint.h>
#endif

#ifdef WIN32
#define snprintf   _snprintf
#define vsnprintf  _vsnprintf
#endif

const uint32_t kMinPkgsInCycleDef      = 20;      // 丢包率计算最少包数的默认值
const uint16_t kMinCalcUintSize        = 10;      // 丢包率计算单元大小的最小值，单位是毫秒
const uint16_t kLostCalcUintSizeDef    = 1000;    // 丢包率计算单元大小的默认值，单位是毫秒
const uint16_t kLostCalcCycleDef       = 5;       // 丢包率计算周期默认值，单位计算单元的个数
const uint16_t kLostCalcDelayDef       = 200;     // 丢包率计算延迟时间默认值，单位是毫秒
const uint16_t kMaxLostCalcUintNum     = 60;      // 丢包率计算单元最大数量，单位是个数

// 丢包率特殊值定义
enum EPkgLostSpecialVal {
    EPLOST_PKG_NUM_TOOL_LITTLE   = 101,  // 计算周期内理论收包数太少
    EPLOST_RECV_NOT_ANY_PKG      = 102,  // 计算周期内未收到任何包
    EPLOST_FOR_INIT_UNKNOWN      = 103,  // 初始化时的未知情况
    EPLOST_INIT_NOT_FROM_SVR     = 104,  // 客户端从未收到过服务器下发的丢包率
    EPLOST_INIT_NOT_FROM_CLI     = 105,  // 服务器端从未收到过客户端上传的丢包率
    EPLOST_INNER_SEQ_ERROR       = 201,  // 内部包序号错误，不能计算
};

// 丢包率计算单元
struct TLostCalcUnit {
    uint32_t start_time;         // 当前计算单元的开始时间，相对于Session开始时间的毫秒差值
    uint32_t start_seq;          // 当前计算单元的起始序号
    uint32_t end_seq;            // 当前计算单元的结束序号
    uint32_t recv_pkgs;          // 当前计算单元的收包数
};


// UDP丢包率计算类，支持“发包+收包”模式、只有收包模式
class CUDPLostCalcEx {
 public:
    CUDPLostCalcEx();
    ~CUDPLostCalcEx();

    // 初始化函数
    //           cur_time：相对于Session开始时间的差值，精确到毫秒
    //     calc_unit_size：丢包率计算单元，单位是毫秒
    //     def_calc_cycle：默认丢包率计算周期，单位计算单元的个数
    //    lost_calc_delay：丢包率计算延迟时间，单位是毫秒
    //  min_pkgs_in_cycle：默认计算周期内要求的最少理论收包数
    //       has_send_pkg：是否有发包，也就是“支持发包+收包”模式，否则为“只有收包”模式
    int Init(uint32_t cur_time, uint16_t calc_unit_size = kLostCalcUintSizeDef,
        uint16_t def_calc_cycle = kLostCalcCycleDef, uint16_t lost_calc_delay = kLostCalcDelayDef,
        uint32_t min_pkgs_in_cycle = kMinPkgsInCycleDef, bool is_send_mode = false);

    // 增加一个发送包或接收包
    //    pkg_seq：当前包序号
    void AddOneRecvPkg(uint32_t pkg_seq);
    void AddOneSendPkg(uint32_t pkg_seq);

    // 计算窗口滑动更新，并触发默认周期的丢包率计算，该函数需要被频繁调用
    //
    //   输入参数：
    //           cur_time：相对于Session开始时间的差值，精确到毫秒
    //
    //             返回值： true表示当次调用做了默认周期的丢包率计算，fasle表示当次调用未做计算
    bool Update(uint32_t cur_time);

    // 指定任意周期的丢包率计算：该函数不能被太频繁调用，否则影响性能
    //
    //   输入参数：
    //            cycle_size：计算周期
    //   end_incomplete_unit：当前未完整的单元是否参与计算
    //
    //    输出参数：
    //     need_recv_pkgs：当前周期理论收包数，当次调用返回true时才有效
    //     real_recv_pkgs：当前周期原始收包数，当次调用返回true时才有效
    //
    //            返回值： 指定周期内的丢包率
    uint8_t CalcAnyCycle(uint16_t cycle_size, uint32_t* need_recv_pkgs, uint32_t* real_recv_pkgs,
        bool end_incomplete_unit = false);

 public:
    // 获取当前丢包率
    inline uint8_t GetDefCurLost(bool ignore_min_pkgs = false) const {
        if (ignore_min_pkgs || (need_recv_pkgs_ >= min_pkgs_in_cycle_) ||
            (cur_lost_def_ > 100)) {
            return cur_lost_def_;
        } else {
            return static_cast<uint8_t>(EPLOST_PKG_NUM_TOOL_LITTLE);
        }
    }

    // 获取当前默认计算周期的理论收包数
    uint32_t GetCurNeedRecvPkgs(void) const {
        return need_recv_pkgs_;
    }

    // 获取当前默认计算周期的实际收包数
    uint32_t GetCurRealRecvPkgs(void) const {
        return real_recv_pkgs_;
    }

    // 获取整个Session中丢包率的平均值
    inline uint8_t GetAllLostAvg(void) const {
        return lost_calc_count_ > 0 ? static_cast<uint8_t>(lost_calc_sum_ / lost_calc_count_) :
            static_cast<uint8_t>(EPLOST_FOR_INIT_UNKNOWN);
    }

    // 获取丢包率计算周期中要求的最少包数值
    inline uint32_t GetMinPkgsInDefCycle(void) const {
        return min_pkgs_in_cycle_;
    }

    // 获取丢包率计算周期时长，单位为计算单位的个数
    inline uint16_t GetDefCalcCycle(void) const {
        return def_calc_cycle_;
    }

    // 获取计算单元大小，单位是毫秒
    inline uint16_t GetCalcUintSize(void) const {
        return calc_unit_size_;
    }

    // 获取丢包率计算延迟时长，单位是毫秒
    inline uint16_t GetLostCalcDelay(void) const {
        return lost_calc_delay_;
    }

    // 获取是否“发包+收包”模式
    bool IsSendMode(void) const {
        return is_send_mode_;
    }

 protected:
     uint8_t CalcCycleLost(uint16_t end_unit_pos, uint16_t cycle_size, uint32_t* need_recv_pkgs,
         uint32_t* real_recv_pkgs);

 protected:
    bool is_send_mode_;           // 是否存在发包，否则只有收包
    bool has_first_pkg_;          // 是否已经收到Session的第一个UDP包
    bool cur_cycle_wait_end_;     // 已经达到当前计算周期结束时间，等待计算

    uint8_t cur_lost_def_;        // 默认周期的当前丢包率
    uint32_t need_recv_pkgs_;     // 当前默认计算周期内应该接收的包数
    uint32_t real_recv_pkgs_;     // 当前默认计算周期内实际接收的包数

    uint16_t calc_unit_size_;     // 计算单元的大小，单位是毫秒
    uint16_t def_calc_cycle_;     // 默认的计算周期，单位为计算单位的个数
    uint16_t lost_calc_delay_;    // 丢包率延时计算时长，单位是毫秒
    uint32_t min_pkgs_in_cycle_;  // 默认计算周中的最少包数阈值

    uint16_t cur_uint_pos_;       // 当前计算单元的位置
    uint16_t next_uint_pos_;      // 下一个计算单元的位置
    TLostCalcUnit calc_unit_arr_[kMaxLostCalcUintNum];  // 丢包率计算单元数组

 protected:
    // 丢包率统计成员
    uint32_t lost_calc_count_;    // 默认计算周期的丢包率计算次数
    uint32_t lost_calc_sum_;      // 默认计算周期的丢包率之和
};

#endif  // TGP_ACCL_TGPCOMM_UDP_LOST_CALC_EX_H
