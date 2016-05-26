
#include "udp_lost_calc_ex.h"

CUDPLostCalcEx::CUDPLostCalcEx() {
    is_send_mode_ = false;
    has_first_pkg_ = false;
    cur_cycle_wait_end_ = false;

    cur_lost_def_ = static_cast<uint8_t>(EPLOST_FOR_INIT_UNKNOWN);
    need_recv_pkgs_ = 0;
    real_recv_pkgs_ = 0;

    calc_unit_size_ = kLostCalcUintSizeDef;
    def_calc_cycle_ = kLostCalcCycleDef;
    lost_calc_delay_ = kLostCalcDelayDef;
    min_pkgs_in_cycle_ = kMinPkgsInCycleDef;

    cur_uint_pos_ = 0;
    next_uint_pos_ = 0;
    ::memset(calc_unit_arr_, 0, sizeof(calc_unit_arr_));

    lost_calc_count_ = 0;
    lost_calc_sum_ = 0;
}

CUDPLostCalcEx::~CUDPLostCalcEx() {
    // Do clean here
}

// 初始化函数
//           cur_time：相对于Session开始时间的差值，精确到毫秒
//     calc_unit_size：丢包率计算单元，单位是毫秒
//     def_calc_cycle：默认丢包率计算周期，单位计算单元的个数
//    lost_calc_delay：丢包率计算延迟时间，单位是毫秒
//  min_pkgs_in_cycle：默认计算周期内要求的最少理论收包数
//       has_send_pkg：是否有发包，也就是“支持发包+收包”模式，否则为“只有收包”模式
int CUDPLostCalcEx::Init(uint32_t cur_time, uint16_t calc_unit_size, uint16_t def_calc_cycle,
                         uint16_t lost_calc_delay, uint32_t min_pkgs_in_cycle, bool is_send_mode) {
    calc_unit_size_ = calc_unit_size;
    def_calc_cycle_ = def_calc_cycle;
    lost_calc_delay_ = lost_calc_delay;
    min_pkgs_in_cycle_ = min_pkgs_in_cycle;
    is_send_mode_ = is_send_mode;
    calc_unit_arr_[0].start_time = cur_time;

    if ((calc_unit_size_ <= kMinCalcUintSize) || (def_calc_cycle_ < 1) ||
        (def_calc_cycle_ > kMaxLostCalcUintNum)) {
        return -1;
    }

    return 0;
}

// 增加一个接收包
//    pkg_seq：当前包序号
void CUDPLostCalcEx::AddOneRecvPkg(uint32_t pkg_seq) {
    TLostCalcUnit& cur_unit = calc_unit_arr_[cur_uint_pos_];
    TLostCalcUnit& next_unit = calc_unit_arr_[next_uint_pos_];

    if (!has_first_pkg_ && !is_send_mode_) {
        // 在当前单元还没有进入延期计算的时间范围内，next_unit与cur_unit是一样的，
        // 因此第一包无条件地加入next_unit而非cur_unit
        has_first_pkg_ = true;
        next_unit.recv_pkgs++;
        next_unit.start_seq = pkg_seq;
        next_unit.end_seq = pkg_seq;
        return;
    }

    // 计算过程中丢弃当前计算单元范围之前的包，32位整数的序号足够大，不考虑包序号回滚的情况
    if (!cur_cycle_wait_end_) {
        if (is_send_mode_) {
            // 只统计当前单元发包序号之间的包，忽略其它范围的包
            if ((pkg_seq >= cur_unit.start_seq) && (pkg_seq <= cur_unit.end_seq)) {
                cur_unit.recv_pkgs++;
            }
        } else {
            if ((pkg_seq >= cur_unit.start_seq) && (pkg_seq <= cur_unit.end_seq)) {
                // 当前包处于当前计算单元的StartSeq和EndSeq之间（乱序包）
                cur_unit.recv_pkgs++;
            } else if (pkg_seq > cur_unit.end_seq) {
                // 当前包处于当前计算单元的EndSeq之后（正常顺序新包）
                cur_unit.recv_pkgs++;
                cur_unit.end_seq = pkg_seq;
            }
        }
    } else {
        if (is_send_mode_) {
            if ((pkg_seq >= cur_unit.start_seq) && (pkg_seq <= cur_unit.end_seq)) {
                cur_unit.recv_pkgs++;
            } else if ((pkg_seq >= next_unit.start_seq) && (pkg_seq <= next_unit.end_seq)) {
                next_unit.recv_pkgs++;
            }
        } else {
            if ((pkg_seq >= cur_unit.start_seq) && (pkg_seq <= cur_unit.end_seq)) {
                // 当前包处于当前计算单元的StartSeq和EndSeq之间（乱序包），计入当前单元
                cur_unit.recv_pkgs++;
            } else if (pkg_seq > cur_unit.end_seq) {
                // 当前包处于当前计算单元的EndSeq之后（正常顺序新包），计入下一个单元
                next_unit.recv_pkgs++;
                if (pkg_seq > next_unit.end_seq) {
                    next_unit.end_seq = pkg_seq;
                }
            }
        }
    }
}

// 增加一个发送包，只针对“发包+收包”模式有效
//    pkg_seq：当前包序号
void CUDPLostCalcEx::AddOneSendPkg(uint32_t pkg_seq) {
    if (!is_send_mode_) {
        // 只针对“发包+收包”模式，才允许调用该函数
        return;
    }

    TLostCalcUnit& cur_unit = calc_unit_arr_[cur_uint_pos_];
    TLostCalcUnit& next_unit = calc_unit_arr_[next_uint_pos_];

    if (!has_first_pkg_) {
        // 在当前单元还没有进入延期计算的时间范围内，next_unit与cur_unit是一样的，
        // 因此第一包无条件地加入next_unit而非cur_unit
        has_first_pkg_ = true;
        next_unit.start_seq = pkg_seq;
        next_unit.end_seq = pkg_seq;
        return;
    }

    // 32位整数的序号足够大，不考虑包序号回滚的情况
    if (!cur_cycle_wait_end_) {
        if (pkg_seq > cur_unit.end_seq) {
            cur_unit.end_seq = pkg_seq;
        } else if (pkg_seq < cur_unit.start_seq) {
            cur_unit.start_seq = pkg_seq;
        }
    } else {
        if (pkg_seq > next_unit.end_seq) {
            next_unit.end_seq = pkg_seq;
        } else if (pkg_seq < next_unit.start_seq) {
            next_unit.start_seq = pkg_seq;
        }
    }
}

// 计算窗口滑动更新，并触发默认周期的丢包率计算，该函数需要被频繁调用
//
//   输入参数：
//           cur_time：相对于Session开始时间的差值，精确到毫秒
//
//             返回值： true表示当次调用做了默认周期的丢包率计算，fasle表示当次调用未做计算
bool CUDPLostCalcEx::Update(uint32_t cur_time) {
    if (cur_cycle_wait_end_) {
        TLostCalcUnit& next_unit = calc_unit_arr_[next_uint_pos_];
        if (cur_time < (next_unit.start_time + lost_calc_delay_)) {
            // 未达到计算时间
            return false;
        }

        cur_lost_def_ = CalcCycleLost(cur_uint_pos_, def_calc_cycle_, &need_recv_pkgs_,
            &real_recv_pkgs_);

        // 丢包率统计
        if (cur_lost_def_ <= 100) {
            lost_calc_count_++;
            lost_calc_sum_ += cur_lost_def_;
        }

        // 计算单元向前推移
        cur_uint_pos_ = next_uint_pos_;
        cur_cycle_wait_end_ = false;
        return true;
    } else {
        // 当前统计单元是否达到结束条件
        TLostCalcUnit& cur_unit = calc_unit_arr_[cur_uint_pos_];
        if (cur_time >= (cur_unit.start_time + calc_unit_size_)) {
            if ((cur_uint_pos_ + 1) < kMaxLostCalcUintNum) {
                next_uint_pos_ = cur_uint_pos_ + 1;
            } else {
                next_uint_pos_ = 0;
            }

            TLostCalcUnit& next_unit = calc_unit_arr_[next_uint_pos_];
            next_unit.start_time = cur_time;
            next_unit.recv_pkgs = 0;
            next_unit.start_seq = cur_unit.end_seq;
            next_unit.end_seq = cur_unit.end_seq;
            cur_cycle_wait_end_ = true;
        }
    }

    // 当次未计算丢包率
    return false;
}

// 指定任意周期的丢包率计算：该函数不能被太频繁调用，否则影响性能
//
//   输入参数：
//            cycle_size：计算周期
// end_incomplete_unit：当前未完整的单元是否参与计算
//
//    输出参数：
//     need_recv_pkgs：当前周期理论收包数，当次调用返回true时才有效
//     real_recv_pkgs：当前周期原始收包数，当次调用返回true时才有效
//
//            返回值： 指定周期内的丢包率
uint8_t CUDPLostCalcEx::CalcAnyCycle(uint16_t cycle_size, uint32_t* need_recv_pkgs,
                                     uint32_t* real_recv_pkgs, bool end_incomplete_unit) {
    // 保证计算的周期不能大于数组的最大值
    if (cycle_size > kMaxLostCalcUintNum) {
        cycle_size = kMaxLostCalcUintNum;
    }

    // 如果不允许当前未完整的单元参与计算，则只计算到当前单元的前一个单元为止
    uint16_t end_unit_pos = cur_uint_pos_;
    if (!end_incomplete_unit) {
        end_unit_pos = (cur_uint_pos_ - 1 + kMaxLostCalcUintNum) % kMaxLostCalcUintNum;
    }

    return CalcCycleLost(end_unit_pos, cycle_size, need_recv_pkgs, real_recv_pkgs);
}

uint8_t CUDPLostCalcEx::CalcCycleLost(uint16_t end_unit_pos, uint16_t cycle_size,
                                      uint32_t* need_recv_pkgs, uint32_t* real_recv_pkgs) {
    uint16_t begin_unit_pos = (end_unit_pos - cycle_size + 1 + kMaxLostCalcUintNum) %
        kMaxLostCalcUintNum;

    TLostCalcUnit& begin_unit = calc_unit_arr_[begin_unit_pos];
    TLostCalcUnit& end_unit = calc_unit_arr_[end_unit_pos];
    if (end_unit.end_seq < begin_unit.start_seq) {
        // 不应该走到这一步
        if (need_recv_pkgs != NULL) {
            *need_recv_pkgs = 0;
        }
        if (real_recv_pkgs != NULL) {
            *real_recv_pkgs = 0;
        }
        return static_cast<uint8_t>(EPLOST_INNER_SEQ_ERROR);
    }

    // 计算理论应收包数和实际收包数，并赋值输出参数
    // 简单起见，忽略Session开始的第一包
    uint32_t should_recv_pkgs = end_unit.end_seq - begin_unit.start_seq;
    if (need_recv_pkgs != NULL) {
        *need_recv_pkgs = should_recv_pkgs;
    }

    uint32_t real_recv_count = 0;
    if (begin_unit_pos <= end_unit_pos) {
        for (uint16_t i = begin_unit_pos; i <= end_unit_pos; i++) {
            real_recv_count += calc_unit_arr_[i].recv_pkgs;
        }
    } else {
        for (uint16_t i = begin_unit_pos; i < kMaxLostCalcUintNum; i++) {
            real_recv_count += calc_unit_arr_[i].recv_pkgs;
        }
        for (uint16_t j = 0; j <= end_unit_pos; j++) {
            real_recv_count += calc_unit_arr_[j].recv_pkgs;
        }
    }

    if (real_recv_pkgs != NULL) {
        *real_recv_pkgs = real_recv_count;
    }

    // 计算丢包率
    uint8_t cycle_lost = 0;
    if (should_recv_pkgs == 0) {
        // 100%丢包或者发包间隔大于计算周期的情况
        cycle_lost = static_cast<uint8_t>(EPLOST_RECV_NOT_ANY_PKG);
    } else if (real_recv_count >= should_recv_pkgs) {
        cycle_lost = 0;
    } else {
        cycle_lost = static_cast<uint8_t>(100 * (should_recv_pkgs - real_recv_count) /
            should_recv_pkgs);
    }

    return cycle_lost;
}

