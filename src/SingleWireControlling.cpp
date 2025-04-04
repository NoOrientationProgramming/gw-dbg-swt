/*
  This file is part of the DSP-Crowd project
  https://www.dsp-crowd.com

  Author(s):
      - Johannes Natter, office@dsp-crowd.com

  File created on 31.03.2025

  Copyright (C) 2025, Johannes Natter

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "SingleWireControlling.h"
#include "LibTime.h"
#include "LibDspc.h"

#include "env.h"

#define dForEach_ProcState(gen) \
		gen(StStart) \
		gen(StUartInit) \
		gen(StDevUartInit) \
		gen(StTargetInit) \
		gen(StTargetInitDoneWait) \
		gen(StNextFlowDetermine) \
		gen(StContentReceive) \

#define dGenProcStateEnum(s) s,
dProcessStateEnum(ProcState);

#if 1
#define dGenProcStateString(s) #s,
dProcessStateStr(ProcState);
#endif

using namespace std;

enum SwtFlowDirection
{
	FlowCtrlToTarget = 0xF1,
	FlowTargetToCtrl
};

enum SwtContentId
{
	ContentNone = 0x00,
	ContentLog = 0x40,
	ContentCmd,
	ContentProc,
};

enum SwtContentIdOut
{
	ContentOutCmd = 0x80,
};

enum SwtContentEnd
{
	ContentEnd = 0x00,
	ContentCut = 0x17,
};

#define dTimeoutTargetInitMs	50
const size_t cSizeFragmentMax = 4095;

SingleWireControlling::SingleWireControlling()
	: Processing("SingleWireControlling")
	, mStartMs(0)
	, mStateRet(StStart)
	, mRefUart(RefDeviceUartInvalid)
	, mDevUartIsOnline(false)
	, mTargetIsOnline(false)
	, mpBuf(NULL)
	, mLenDone(0)
	, mFragments()
	, mContentCurrent(ContentNone)
{
	mResp.idContent = ContentNone;
	mResp.content = "";

	mState = StStart;
}

/* member functions */

Success SingleWireControlling::process()
{
	//uint32_t curTimeMs = millis();
	//uint32_t diffMs = curTimeMs - mStartMs;
	Success success;
#if 0
	dStateTrace;
#endif
	switch (mState)
	{
	case StStart:

		mState = StUartInit;

		break;
	case StUartInit:

		mDevUartIsOnline = false;

		mState = StDevUartInit;

		break;
	case StDevUartInit:

		success = devUartInit(env.deviceUart, mRefUart);
		if (success == Pending)
			break;

		if (success != Positive)
			return procErrLog(-1, "could not initalize UART device");

		// clear UART device buffer?

		mDevUartIsOnline = true;

		mState = StTargetInit;

		break;
	case StTargetInit:

		mTargetIsOnline = false;

		cmdSend("aaaaa");
		dataRequest();

		mState = StTargetInitDoneWait;

		break;
	case StTargetInitDoneWait:

		success = dataReceive();
		if (success == Pending)
			break;

		if (success == SwtErrRcvNoUart)
		{
			mState = StUartInit;
			break;
		}

		if (success == SwtErrRcvNoTarget)
		{
			mState = StTargetInit;
			break;
		}

		hexDump(mBufRcv, mLenDone);

		mTargetIsOnline = true;

		mState = StNextFlowDetermine;

		break;
	case StNextFlowDetermine:

		// flow determine
#if 0
		dataRequest();
#endif
		break;
	case StContentReceive:

		break;
	default:
		break;
	}

	return Pending;
}

void SingleWireControlling::cmdSend(const string &cmd)
{
	uartSend(mRefUart, FlowCtrlToTarget);
	uartSend(mRefUart, ContentOutCmd);
	uartSend(mRefUart, cmd);
	uartSend(mRefUart, 0x00);

	mStartMs = millis();
}

void SingleWireControlling::dataRequest()
{
	uartSend(mRefUart, FlowTargetToCtrl);
	mStartMs = millis();
}

Success SingleWireControlling::dataReceive()
{
	uint32_t curTimeMs = millis();
	uint32_t diffMs = curTimeMs - mStartMs;

	if (diffMs > dTimeoutTargetInitMs)
		return -SwtErrRcvNoTarget;

	mLenDone = uartRead(mRefUart, mBufRcv, sizeof(mBufRcv));
	if (!mLenDone)
		return Pending;

	if (mLenDone < 0)
		return -SwtErrRcvNoUart;

	mpBuf = mBufRcv;

	Success success;

	for (; mLenDone; --mLenDone, ++mpBuf)
	{
		success = byteProcess(*mpBuf);
		if (success == Pending)
			continue;

		if (success != Positive)
			return -SwtErrRcvProtocol;

		return Positive;
	}

	return Pending;
}

Success SingleWireControlling::byteProcess(char ch)
{
	(void)ch;
#if 0
	if (*pBuf >= ContentLog && *pBuf <= ContentProc)
	{
		mContentCurrent = *pBuf;

		++pBuf;
		--mLenDone;
	}

	if (*pEnd == ContentEnd)
	{
		fragmentFinish(pBuf, mLenDone);
		return Positive;
	}

	fragmentAppend(pBuf, mLenDone);
#endif
	return Pending;
}

void SingleWireControlling::fragmentAppend(const char *pBuf, size_t len)
{
	bool fragmentFound =
			mFragments.find(mContentCurrent) != mFragments.end();

	if (!fragmentFound)
	{
		mFragments[mContentCurrent] = string(pBuf, len);
		return;
	}

	string &str = mFragments[mContentCurrent];

	if (str.size() > cSizeFragmentMax)
		return;

	mFragments[mContentCurrent] += string(pBuf, len);
}

void SingleWireControlling::fragmentFinish(const char *pBuf, size_t len)
{
	bool fragmentFound =
			mFragments.find(mContentCurrent) != mFragments.end();

	mResp.idContent = mContentCurrent;
	mResp.content = "";

	if (fragmentFound)
	{
		mResp.content = mFragments[mContentCurrent];
		mFragments.erase(mContentCurrent);
	};

	mResp.content += string(pBuf, len);

	mContentCurrent = ContentNone;
}

void SingleWireControlling::processInfo(char *pBuf, char *pBufEnd)
{
#if 1
	dInfo("State\t\t\t%s\n", ProcStateString[mState]);
#endif
	dInfo("UART: %s\t%sline\n",
			env.deviceUart.c_str(),
			mDevUartIsOnline ? "On" : "Off");
	dInfo("Target\t\t\t%sline\n", mTargetIsOnline ? "On" : "Off");
}

/* static functions */

