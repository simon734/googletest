
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

// ��ʼ������
//           cur_time�������Session��ʼʱ��Ĳ�ֵ����ȷ������
//     calc_unit_size�������ʼ��㵥Ԫ����λ�Ǻ���
//     def_calc_cycle��Ĭ�϶����ʼ������ڣ���λ���㵥Ԫ�ĸ���
//    lost_calc_delay�������ʼ����ӳ�ʱ�䣬��λ�Ǻ���
//  min_pkgs_in_cycle��Ĭ�ϼ���������Ҫ������������հ���
//       has_send_pkg���Ƿ��з�����Ҳ���ǡ�֧�ַ���+�հ���ģʽ������Ϊ��ֻ���հ���ģʽ
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

// ����һ�����հ�
//    pkg_seq����ǰ�����
void CUDPLostCalcEx::AddOneRecvPkg(uint32_t pkg_seq) {
    TLostCalcUnit& cur_unit = calc_unit_arr_[cur_uint_pos_];
    TLostCalcUnit& next_unit = calc_unit_arr_[next_uint_pos_];

    if (!has_first_pkg_ && !is_send_mode_) {
        // �ڵ�ǰ��Ԫ��û�н������ڼ����ʱ�䷶Χ�ڣ�next_unit��cur_unit��һ���ģ�
        // ��˵�һ���������ؼ���next_unit����cur_unit
        has_first_pkg_ = true;
        next_unit.recv_pkgs++;
        next_unit.start_seq = pkg_seq;
        next_unit.end_seq = pkg_seq;
        return;
    }

    // ��������ж�����ǰ���㵥Ԫ��Χ֮ǰ�İ���32λ����������㹻�󣬲����ǰ���Żع������
    if (!cur_cycle_wait_end_) {
        if (is_send_mode_) {
            // ֻͳ�Ƶ�ǰ��Ԫ�������֮��İ�������������Χ�İ�
            if ((pkg_seq >= cur_unit.start_seq) && (pkg_seq <= cur_unit.end_seq)) {
                cur_unit.recv_pkgs++;
            }
        } else {
            if ((pkg_seq >= cur_unit.start_seq) && (pkg_seq <= cur_unit.end_seq)) {
                // ��ǰ�����ڵ�ǰ���㵥Ԫ��StartSeq��EndSeq֮�䣨�������
                cur_unit.recv_pkgs++;
            } else if (pkg_seq > cur_unit.end_seq) {
                // ��ǰ�����ڵ�ǰ���㵥Ԫ��EndSeq֮������˳���°���
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
                // ��ǰ�����ڵ�ǰ���㵥Ԫ��StartSeq��EndSeq֮�䣨������������뵱ǰ��Ԫ
                cur_unit.recv_pkgs++;
            } else if (pkg_seq > cur_unit.end_seq) {
                // ��ǰ�����ڵ�ǰ���㵥Ԫ��EndSeq֮������˳���°�����������һ����Ԫ
                next_unit.recv_pkgs++;
                if (pkg_seq > next_unit.end_seq) {
                    next_unit.end_seq = pkg_seq;
                }
            }
        }
    }
}

// ����һ�����Ͱ���ֻ��ԡ�����+�հ���ģʽ��Ч
//    pkg_seq����ǰ�����
void CUDPLostCalcEx::AddOneSendPkg(uint32_t pkg_seq) {
    if (!is_send_mode_) {
        // ֻ��ԡ�����+�հ���ģʽ����������øú���
        return;
    }

    TLostCalcUnit& cur_unit = calc_unit_arr_[cur_uint_pos_];
    TLostCalcUnit& next_unit = calc_unit_arr_[next_uint_pos_];

    if (!has_first_pkg_) {
        // �ڵ�ǰ��Ԫ��û�н������ڼ����ʱ�䷶Χ�ڣ�next_unit��cur_unit��һ���ģ�
        // ��˵�һ���������ؼ���next_unit����cur_unit
        has_first_pkg_ = true;
        next_unit.start_seq = pkg_seq;
        next_unit.end_seq = pkg_seq;
        return;
    }

    // 32λ����������㹻�󣬲����ǰ���Żع������
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

// ���㴰�ڻ������£�������Ĭ�����ڵĶ����ʼ��㣬�ú�����Ҫ��Ƶ������
//
//   ���������
//           cur_time�������Session��ʼʱ��Ĳ�ֵ����ȷ������
//
//             ����ֵ�� true��ʾ���ε�������Ĭ�����ڵĶ����ʼ��㣬fasle��ʾ���ε���δ������
bool CUDPLostCalcEx::Update(uint32_t cur_time) {
    if (cur_cycle_wait_end_) {
        TLostCalcUnit& next_unit = calc_unit_arr_[next_uint_pos_];
        if (cur_time < (next_unit.start_time + lost_calc_delay_)) {
            // δ�ﵽ����ʱ��
            return false;
        }

        cur_lost_def_ = CalcCycleLost(cur_uint_pos_, def_calc_cycle_, &need_recv_pkgs_,
            &real_recv_pkgs_);

        // ������ͳ��
        if (cur_lost_def_ <= 100) {
            lost_calc_count_++;
            lost_calc_sum_ += cur_lost_def_;
        }

        // ���㵥Ԫ��ǰ����
        cur_uint_pos_ = next_uint_pos_;
        cur_cycle_wait_end_ = false;
        return true;
    } else {
        // ��ǰͳ�Ƶ�Ԫ�Ƿ�ﵽ��������
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

    // ����δ���㶪����
    return false;
}

// ָ���������ڵĶ����ʼ��㣺�ú������ܱ�̫Ƶ�����ã�����Ӱ������
//
//   ���������
//            cycle_size����������
// end_incomplete_unit����ǰδ�����ĵ�Ԫ�Ƿ�������
//
//    ���������
//     need_recv_pkgs����ǰ���������հ��������ε��÷���trueʱ����Ч
//     real_recv_pkgs����ǰ����ԭʼ�հ��������ε��÷���trueʱ����Ч
//
//            ����ֵ�� ָ�������ڵĶ�����
uint8_t CUDPLostCalcEx::CalcAnyCycle(uint16_t cycle_size, uint32_t* need_recv_pkgs,
                                     uint32_t* real_recv_pkgs, bool end_incomplete_unit) {
    // ��֤��������ڲ��ܴ�����������ֵ
    if (cycle_size > kMaxLostCalcUintNum) {
        cycle_size = kMaxLostCalcUintNum;
    }

    // ���������ǰδ�����ĵ�Ԫ������㣬��ֻ���㵽��ǰ��Ԫ��ǰһ����ԪΪֹ
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
        // ��Ӧ���ߵ���һ��
        if (need_recv_pkgs != NULL) {
            *need_recv_pkgs = 0;
        }
        if (real_recv_pkgs != NULL) {
            *real_recv_pkgs = 0;
        }
        return static_cast<uint8_t>(EPLOST_INNER_SEQ_ERROR);
    }

    // ��������Ӧ�հ�����ʵ���հ���������ֵ�������
    // �����������Session��ʼ�ĵ�һ��
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

    // ���㶪����
    uint8_t cycle_lost = 0;
    if (should_recv_pkgs == 0) {
        // 100%�������߷���������ڼ������ڵ����
        cycle_lost = static_cast<uint8_t>(EPLOST_RECV_NOT_ANY_PKG);
    } else if (real_recv_count >= should_recv_pkgs) {
        cycle_lost = 0;
    } else {
        cycle_lost = static_cast<uint8_t>(100 * (should_recv_pkgs - real_recv_count) /
            should_recv_pkgs);
    }

    return cycle_lost;
}

