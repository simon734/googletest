#ifndef TGP_ACCL_TGPCOMM_UDP_LOST_CALC_EX_H
#define TGP_ACCL_TGPCOMM_UDP_LOST_CALC_EX_H
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// �����������Ͷ���
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

const uint32_t kMinPkgsInCycleDef      = 20;      // �����ʼ������ٰ�����Ĭ��ֵ
const uint16_t kMinCalcUintSize        = 10;      // �����ʼ��㵥Ԫ��С����Сֵ����λ�Ǻ���
const uint16_t kLostCalcUintSizeDef    = 1000;    // �����ʼ��㵥Ԫ��С��Ĭ��ֵ����λ�Ǻ���
const uint16_t kLostCalcCycleDef       = 5;       // �����ʼ�������Ĭ��ֵ����λ���㵥Ԫ�ĸ���
const uint16_t kLostCalcDelayDef       = 200;     // �����ʼ����ӳ�ʱ��Ĭ��ֵ����λ�Ǻ���
const uint16_t kMaxLostCalcUintNum     = 60;      // �����ʼ��㵥Ԫ�����������λ�Ǹ���

// ����������ֵ����
enum EPkgLostSpecialVal {
    EPLOST_PKG_NUM_TOOL_LITTLE   = 101,  // ���������������հ���̫��
    EPLOST_RECV_NOT_ANY_PKG      = 102,  // ����������δ�յ��κΰ�
    EPLOST_FOR_INIT_UNKNOWN      = 103,  // ��ʼ��ʱ��δ֪���
    EPLOST_INIT_NOT_FROM_SVR     = 104,  // �ͻ��˴�δ�յ����������·��Ķ�����
    EPLOST_INIT_NOT_FROM_CLI     = 105,  // �������˴�δ�յ����ͻ����ϴ��Ķ�����
    EPLOST_INNER_SEQ_ERROR       = 201,  // �ڲ�����Ŵ��󣬲��ܼ���
};

// �����ʼ��㵥Ԫ
struct TLostCalcUnit {
    uint32_t start_time;         // ��ǰ���㵥Ԫ�Ŀ�ʼʱ�䣬�����Session��ʼʱ��ĺ����ֵ
    uint32_t start_seq;          // ��ǰ���㵥Ԫ����ʼ���
    uint32_t end_seq;            // ��ǰ���㵥Ԫ�Ľ������
    uint32_t recv_pkgs;          // ��ǰ���㵥Ԫ���հ���
};


// UDP�����ʼ����֧࣬�֡�����+�հ���ģʽ��ֻ���հ�ģʽ
class CUDPLostCalcEx {
 public:
    CUDPLostCalcEx();
    ~CUDPLostCalcEx();

    // ��ʼ������
    //           cur_time�������Session��ʼʱ��Ĳ�ֵ����ȷ������
    //     calc_unit_size�������ʼ��㵥Ԫ����λ�Ǻ���
    //     def_calc_cycle��Ĭ�϶����ʼ������ڣ���λ���㵥Ԫ�ĸ���
    //    lost_calc_delay�������ʼ����ӳ�ʱ�䣬��λ�Ǻ���
    //  min_pkgs_in_cycle��Ĭ�ϼ���������Ҫ������������հ���
    //       has_send_pkg���Ƿ��з�����Ҳ���ǡ�֧�ַ���+�հ���ģʽ������Ϊ��ֻ���հ���ģʽ
    int Init(uint32_t cur_time, uint16_t calc_unit_size = kLostCalcUintSizeDef,
        uint16_t def_calc_cycle = kLostCalcCycleDef, uint16_t lost_calc_delay = kLostCalcDelayDef,
        uint32_t min_pkgs_in_cycle = kMinPkgsInCycleDef, bool is_send_mode = false);

    // ����һ�����Ͱ�����հ�
    //    pkg_seq����ǰ�����
    void AddOneRecvPkg(uint32_t pkg_seq);
    void AddOneSendPkg(uint32_t pkg_seq);

    // ���㴰�ڻ������£�������Ĭ�����ڵĶ����ʼ��㣬�ú�����Ҫ��Ƶ������
    //
    //   ���������
    //           cur_time�������Session��ʼʱ��Ĳ�ֵ����ȷ������
    //
    //             ����ֵ�� true��ʾ���ε�������Ĭ�����ڵĶ����ʼ��㣬fasle��ʾ���ε���δ������
    bool Update(uint32_t cur_time);

    // ָ���������ڵĶ����ʼ��㣺�ú������ܱ�̫Ƶ�����ã�����Ӱ������
    //
    //   ���������
    //            cycle_size����������
    //   end_incomplete_unit����ǰδ�����ĵ�Ԫ�Ƿ�������
    //
    //    ���������
    //     need_recv_pkgs����ǰ���������հ��������ε��÷���trueʱ����Ч
    //     real_recv_pkgs����ǰ����ԭʼ�հ��������ε��÷���trueʱ����Ч
    //
    //            ����ֵ�� ָ�������ڵĶ�����
    uint8_t CalcAnyCycle(uint16_t cycle_size, uint32_t* need_recv_pkgs, uint32_t* real_recv_pkgs,
        bool end_incomplete_unit = false);

 public:
    // ��ȡ��ǰ������
    inline uint8_t GetDefCurLost(bool ignore_min_pkgs = false) const {
        if (ignore_min_pkgs || (need_recv_pkgs_ >= min_pkgs_in_cycle_) ||
            (cur_lost_def_ > 100)) {
            return cur_lost_def_;
        } else {
            return static_cast<uint8_t>(EPLOST_PKG_NUM_TOOL_LITTLE);
        }
    }

    // ��ȡ��ǰĬ�ϼ������ڵ������հ���
    uint32_t GetCurNeedRecvPkgs(void) const {
        return need_recv_pkgs_;
    }

    // ��ȡ��ǰĬ�ϼ������ڵ�ʵ���հ���
    uint32_t GetCurRealRecvPkgs(void) const {
        return real_recv_pkgs_;
    }

    // ��ȡ����Session�ж����ʵ�ƽ��ֵ
    inline uint8_t GetAllLostAvg(void) const {
        return lost_calc_count_ > 0 ? static_cast<uint8_t>(lost_calc_sum_ / lost_calc_count_) :
            static_cast<uint8_t>(EPLOST_FOR_INIT_UNKNOWN);
    }

    // ��ȡ�����ʼ���������Ҫ������ٰ���ֵ
    inline uint32_t GetMinPkgsInDefCycle(void) const {
        return min_pkgs_in_cycle_;
    }

    // ��ȡ�����ʼ�������ʱ������λΪ���㵥λ�ĸ���
    inline uint16_t GetDefCalcCycle(void) const {
        return def_calc_cycle_;
    }

    // ��ȡ���㵥Ԫ��С����λ�Ǻ���
    inline uint16_t GetCalcUintSize(void) const {
        return calc_unit_size_;
    }

    // ��ȡ�����ʼ����ӳ�ʱ������λ�Ǻ���
    inline uint16_t GetLostCalcDelay(void) const {
        return lost_calc_delay_;
    }

    // ��ȡ�Ƿ񡰷���+�հ���ģʽ
    bool IsSendMode(void) const {
        return is_send_mode_;
    }

 protected:
     uint8_t CalcCycleLost(uint16_t end_unit_pos, uint16_t cycle_size, uint32_t* need_recv_pkgs,
         uint32_t* real_recv_pkgs);

 protected:
    bool is_send_mode_;           // �Ƿ���ڷ���������ֻ���հ�
    bool has_first_pkg_;          // �Ƿ��Ѿ��յ�Session�ĵ�һ��UDP��
    bool cur_cycle_wait_end_;     // �Ѿ��ﵽ��ǰ�������ڽ���ʱ�䣬�ȴ�����

    uint8_t cur_lost_def_;        // Ĭ�����ڵĵ�ǰ������
    uint32_t need_recv_pkgs_;     // ��ǰĬ�ϼ���������Ӧ�ý��յİ���
    uint32_t real_recv_pkgs_;     // ��ǰĬ�ϼ���������ʵ�ʽ��յİ���

    uint16_t calc_unit_size_;     // ���㵥Ԫ�Ĵ�С����λ�Ǻ���
    uint16_t def_calc_cycle_;     // Ĭ�ϵļ������ڣ���λΪ���㵥λ�ĸ���
    uint16_t lost_calc_delay_;    // ��������ʱ����ʱ������λ�Ǻ���
    uint32_t min_pkgs_in_cycle_;  // Ĭ�ϼ������е����ٰ�����ֵ

    uint16_t cur_uint_pos_;       // ��ǰ���㵥Ԫ��λ��
    uint16_t next_uint_pos_;      // ��һ�����㵥Ԫ��λ��
    TLostCalcUnit calc_unit_arr_[kMaxLostCalcUintNum];  // �����ʼ��㵥Ԫ����

 protected:
    // ������ͳ�Ƴ�Ա
    uint32_t lost_calc_count_;    // Ĭ�ϼ������ڵĶ����ʼ������
    uint32_t lost_calc_sum_;      // Ĭ�ϼ������ڵĶ�����֮��
};

#endif  // TGP_ACCL_TGPCOMM_UDP_LOST_CALC_EX_H
