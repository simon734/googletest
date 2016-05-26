// Copyright 2006, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <stdio.h>

#include "gtest/gtest.h"
#include <Windows.h>
#include "udp_lost_calc_ex.h"

struct TestControl
{
	// 收发包间隔时间
	int send_recv_interval;

	// 丢包概率
	int lost_ratio;

	// 计算周期
	int calc_interval;

	// 测试时长
	unsigned long calc_count;
};

struct data_stat
{
	// 发包数
	int send_count;

	// 丢包数
	int recv_count;
};

struct udp_lost_calc_control: testing::Test, testing::WithParamInterface<TestControl>
{
	CUDPLostCalcEx lost_calc;
	data_stat stat[5] = {};

	udp_lost_calc_control()
	{
		lost_calc.Init(0);
		::memset(stat, 0, sizeof(stat));
	}
};

TEST_P(udp_lost_calc_control, ControlTest) {
	auto calc_test = GetParam();

	unsigned int last_tick = ::GetTickCount();
	while(calc_test.calc_count > 0) {
		::Sleep(calc_test.send_recv_interval);
		unsigned int tick_interval = ::GetTickCount() - last_tick;
		if (last_tick >= calc_test.calc_interval)
		{
			last_tick = ::GetTickCount();
			calc_test.calc_count--;
		}
	}
	EXPECT_EQ(stat[0].recv_count, stat[0].send_count);
}

INSTANTIATE_TEST_CASE_P(Default, udp_lost_calc_control,
	testing::Values(
		TestControl{ 15, 5, 1000, 10 },
		TestControl{ 15, 2, 1000, 10 },
		TestControl{ 20, 10, 1000, 10 }
	));

GTEST_API_ int main(int argc, char **argv) {
  printf("Running main() from gtest_main.cc\n");
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
