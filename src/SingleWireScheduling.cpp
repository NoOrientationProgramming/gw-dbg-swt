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

#include "SingleWireScheduling.h"
#include "SystemDebugging.h"
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
		gen(StContentReceiveWait) \
		gen(StCtrlManual) \

#define dGenProcStateEnum(s) s,
dProcessStateEnum(ProcState);

#if 1
#define dGenProcStateString(s) #s,
dProcessStateStr(ProcState);
#endif

#define dForEach_SwtState(gen) \
		gen(StSwtContentRcvWait) \
		gen(StSwtDataReceive) \

#define dGenSwtStateEnum(s) s,
dProcessStateEnum(SwtState);

#if 1
#define dGenSwtStateString(s) #s,
dProcessStateStr(SwtState);
#endif

using namespace std;

enum SwtFlowDirection
{
	FlowCtrlToTarget = 0x0B,
	FlowTargetToCtrl = 0x0C
};

enum SwtContentIdOut
{
	IdContentOutCmd = 0x1A,
};

enum SwtContentEnd
{
	IdContentCut = 0x0F,
	IdContentEnd = 0x17,
};

#define dTimeoutTargetInitMs	35
const size_t cSizeFragmentMax = 4095;
const uint8_t cKeyEscape = 0x1B;
const uint8_t cKeyTab = '\t';
const uint8_t cKeyCr = '\r';
const uint8_t cKeyLf = '\n';

static uint8_t uartVirtualTimeout = 0;
RefDeviceUart refUart;

const size_t cNumRequestsCmdMax = 40;
const uint32_t cTimeoutCmduC = 100;
const uint32_t cTimeoutCmdReq = 5500;

list<CommandReqResp> SingleWireScheduling::requestsCmd[3];
list<CommandReqResp> SingleWireScheduling::responsesCmd;
uint32_t SingleWireScheduling::idReqCmdNext = 0;

SingleWireScheduling::SingleWireScheduling()
	: Processing("SingleWireScheduling")
	, mDevUartIsOnline(false)
	, mTargetIsOnline(false)
	, mContentProc("")
	, mStateSwt(StSwtContentRcvWait)
	, mStartMs(0)
	, mRefUart(RefDeviceUartInvalid)
	, mpBuf(NULL)
	, mLenDone(0)
	, mFragments()
	, mContentProcChanged(false)
	, mCntBytesRcvd(0)
	, mCntContentNoneRcvd(0)
	, mLastProcTreeRcvdMs(0)
	, mTargetIsOnlineOld(true)
	, mTargetIsOfflineMarked(false)
	, mContentIgnore(false)
	, mpListCmdCurrent(NULL)
	, mCntDelayPrioLow(0)
	, mStartCmdMs(0)
{
	responseReset();
	mBufRcv[0] = 0;

	mState = StStart;
}

/* member functions */

Success SingleWireScheduling::process()
{
	uint32_t curTimeMs = millis();
	//uint32_t diffMs = curTimeMs - mStartMs;
	Success success;
	bool ok;
#if 0
	dStateTrace;
#endif
	switch (mState)
	{
	case StStart:

		cmdReg("ctrlManualToggle", cmdCtrlManualToggle,      "",  "Toggle manual control",               "Manual Control");
		cmdReg("dataUartSend",     cmdDataUartSend,          "",  "Send byte stream",                    "Manual Control");
		cmdReg("strUartSend",      cmdStrUartSend,           "",  "Send string",                         "Manual Control");
		cmdReg("dataUartRead",     cmdDataUartRead,          "",  "Read data",                           "Manual Control");
		cmdReg("modeUartVirtSet",  cmdModeUartVirtSet,       "",  "Mode: uart, swart (default)",         "Virtual UART");
		cmdReg("uartVirtToggle",   cmdUartVirtToggle,        "",  "Enable/Disable virtual UART",         "Virtual UART");
		cmdReg("mountedToggle",    cmdMountedUartVirtToggle, "m", "Mount/Unmount virtual UART",          "Virtual UART");
		cmdReg("timeoutToggle",    cmdTimeoutUartVirtToggle, "t", "Enable/Disable virtual UART timeout", "Virtual UART");
		cmdReg("dataUartRcv",      cmdDataUartRcv,           "",  "Receive byte stream",                 "Virtual UART");
		cmdReg("strUartRcv",       cmdStrUartRcv,            "",  "Receive string",                      "Virtual UART");
		cmdReg("cmdSend",          cmdCommandSend,           "",  "Send command",                        "Commands");

		mState = StUartInit;

		break;
	case StUartInit:

		if (mRefUart != RefDeviceUartInvalid)
		{
			devUartDeInit(mRefUart);
			refUart = mRefUart;
		}

		mDevUartIsOnline = false;
		targetOnlineSet(false);

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
		refUart = mRefUart;

		mState = StTargetInit;

		break;
	case StTargetInit:

		targetOnlineSet(false);

		if (env.ctrlManual)
		{
			mState = StCtrlManual;
			break;
		}

		cmdSend(env.codeUart);

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
#if 0
		procWrnLog("content received: %02X > '%s'",
						mResp.idContent,
						mResp.content.c_str());
#endif
		if (mResp.idContent != IdContentCmd)
			break;

		if (mResp.content != "Debug mode 1")
			break;

		targetOnlineSet();

		mpListCmdCurrent = NULL;
		mStartCmdMs = 0;

		mState = StNextFlowDetermine;

		break;
	case StNextFlowDetermine:

		if (env.ctrlManual)
		{
			mState = StCtrlManual;
			break;
		}

		commandsCheck(curTimeMs);

		// flow determine

		ok = cmdQueueCheck();
		if (ok)
			break;

		dataRequest();
		mState = StContentReceiveWait;

		break;
	case StContentReceiveWait:

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
#if 0
		if (mResp.idContent != IdContentNone)
		{
			procWrnLog("content received: %02X",
						mResp.idContent);
#if 0
			procWrnLog("'%s'", mResp.content.c_str());
#endif
		}
#endif
		if (mResp.idContent == IdContentProc &&
				mContentProc != mResp.content)
		{
			mTargetIsOfflineMarked = false;

			mContentProc = mResp.content;
			mContentProcChanged = true;
		}

		if (mResp.idContent == IdContentLog)
			ppEntriesLog.commit(mResp.content);

		if (mResp.idContent == IdContentCmd)
			cmdResponseReceived(mResp.content);

		responseReset();

		mState = StNextFlowDetermine;

		break;
	case StCtrlManual:

		if (!env.ctrlManual)
		{
			mState = StTargetInit;
			break;
		}

		break;
	default:
		break;
	}

	return Pending;
}

bool SingleWireScheduling::contentProcChanged()
{
	bool tmp = mContentProcChanged;
	mContentProcChanged = false;
	return tmp;
}

bool SingleWireScheduling::cmdQueueCheck()
{
	if (mpListCmdCurrent)
		return false;

	if (requestsCmd[PrioUser].size())
		mpListCmdCurrent = &requestsCmd[PrioUser];
	else
	if (requestsCmd[PrioSysLow].size())
	{
		if (mCntDelayPrioLow)
			return false;

		mpListCmdCurrent = &requestsCmd[PrioSysLow];
		mCntDelayPrioLow = 4;
	}

	if (!mpListCmdCurrent)
		return false;
	mStartCmdMs = millis();

	const CommandReqResp *pReq = &mpListCmdCurrent->front();
	cmdSend(pReq->str);

	return true;
}

void SingleWireScheduling::cmdResponseReceived(const string &resp)
{
	if (!mpListCmdCurrent)
		return;
#if 0
	procWrnLog("command response received: %s",
				resp.c_str());
#endif
	uint32_t idReq = mpListCmdCurrent->front().idReq;
	mpListCmdCurrent->pop_front();
	responsesCmd.emplace_back(resp, idReq, millis());

	mpListCmdCurrent = NULL;
}

void SingleWireScheduling::commandsCheck(uint32_t curTimeMs)
{
	cmdResponsesClear(curTimeMs);

	if (!mpListCmdCurrent)
		return;

	uint32_t diffMs = curTimeMs - mStartCmdMs;

	if (diffMs > cTimeoutCmduC)
		mpListCmdCurrent = NULL;
}

void SingleWireScheduling::cmdResponsesClear(uint32_t curTimeMs)
{
	list<CommandReqResp>::iterator iter;
	uint32_t diffMs;

	iter = responsesCmd.begin();
	while (iter != responsesCmd.end())
	{
		diffMs = curTimeMs - iter->startMs;

		if (diffMs < cTimeoutCmdReq)
		{
			++iter;
			continue;
		}
#if 0
		procWrnLog("response timeout for: %u",
					iter->idReq);
#endif
		iter = responsesCmd.erase(iter);
	}
}

void SingleWireScheduling::cmdSend(const string &cmd)
{
	uartSend(mRefUart, FlowCtrlToTarget);
	uartSend(mRefUart, IdContentOutCmd);
	uartSend(mRefUart, cmd.data(), cmd.size());
	uartSend(mRefUart, 0x00);
	uartSend(mRefUart, IdContentEnd);

	mStartMs = millis();

	//procWrnLog("cmd sent: %s", cmd.c_str());
}

void SingleWireScheduling::dataRequest()
{
	uartSend(mRefUart, FlowTargetToCtrl);
	mStartMs = millis();

	//procWrnLog("data requested");

	if (mCntDelayPrioLow)
	{
		--mCntDelayPrioLow;
		//procWrnLog("low prio delay: %u", mCntDelayPrioLow);
	}
}

Success SingleWireScheduling::dataReceive()
{
	if (mLenDone > 0)
		mStartMs = millis();

	uint32_t curTimeMs = millis();
	uint32_t diffMs = curTimeMs - mStartMs;
	Success success;

	while (mLenDone > 0)
	{
		success = byteProcess((uint8_t)*mpBuf, curTimeMs);

		++mpBuf;
		--mLenDone;

		if (success == Positive)
			return Positive;
	}

	//procInfLog("diff: %u <=> %u", diffMs, dTimeoutTargetInitMs);

	if ((!uartVirtual && diffMs > dTimeoutTargetInitMs) ||
		(uartVirtual && uartVirtualTimeout))
	{
		mFragments.clear();
		mStateSwt = StSwtContentRcvWait;

		return SwtErrRcvNoTarget;
	}

	mLenDone = uartRead(mRefUart, mBufRcv, sizeof(mBufRcv));
	if (!mLenDone)
		return Pending;

	if (mLenDone < 0)
	{
		mFragments.clear();
		mStateSwt = StSwtContentRcvWait;

		return SwtErrRcvNoUart;
	}

	mpBuf = mBufRcv;

	mStartMs = millis();

	return Pending;
}

Success SingleWireScheduling::byteProcess(uint8_t ch, uint32_t curTimeMs)
{
	uint32_t diffMs = curTimeMs - mLastProcTreeRcvdMs;
#if 0
	procInfLog("received byte in %s: 0x%02X '%c'",
				SwtStateString[mStateSwt], ch, ch);
#endif
	++mCntBytesRcvd;

	switch (mStateSwt)
	{
	case StSwtContentRcvWait:

		if (ch == IdContentNone)
		{
			//procWrnLog("received IdContentNone");
			++mCntContentNoneRcvd;

			responseReset();

			return Positive;
		}

		if (ch < IdContentProc || ch > IdContentCmd)
			break;

		responseReset(ch);
		mContentIgnore = false;

		if (ch != IdContentProc)
		{
			mStateSwt = StSwtDataReceive;
			break;
		}

		// Process Tree filter

		if (diffMs > env.rateRefreshMs)
		{
			mLastProcTreeRcvdMs = curTimeMs;
			mStateSwt = StSwtDataReceive;
			break;
		}

		responseReset();
		mContentIgnore = true;

		mStateSwt = StSwtDataReceive;

		break;
	case StSwtDataReceive:

		if (ch == IdContentCut)
		{
			mStateSwt = StSwtContentRcvWait;
			break;
		}

		if (ch == IdContentEnd)
		{
			if (!mContentIgnore)
				fragmentFinish();

			mStateSwt = StSwtContentRcvWait;
			return Positive;
		}

		if (!ch)
			break;

		if (isprint(ch) ||
			ch == cKeyEscape ||
			ch == cKeyTab ||
			ch == cKeyCr ||
			ch == cKeyLf)
		{
			if (!mContentIgnore)
				fragmentAppend(ch);
			break;
		}

		if (!mContentIgnore)
			fragmentDelete();

		mStateSwt = StSwtContentRcvWait;
		return SwtErrRcvProtocol;

		break;
	default:
		break;
	}

	return Pending;
}

Success SingleWireScheduling::shutdown()
{

	if (mRefUart != RefDeviceUartInvalid)
	{
		devUartDeInit(mRefUart);
		refUart = mRefUart;
	}

	return Positive;
}

void SingleWireScheduling::fragmentAppend(uint8_t ch)
{
	if (!ch)
		return;

	uint8_t idContent = mResp.idContent;
	bool fragmentFound =
			mFragments.find(idContent) != mFragments.end();

	if (!fragmentFound)
	{
		mFragments[idContent] = string(1, ch);
		return;
	}

	const string &str = mFragments[idContent];

	if (str.size() > cSizeFragmentMax)
		return;

	mFragments[idContent] += string(1, ch);
}

void SingleWireScheduling::fragmentFinish()
{
	uint8_t idContent = mResp.idContent;
	bool fragmentFound =
			mFragments.find(idContent) != mFragments.end();

	if (!fragmentFound)
		return;

	mResp.content = mFragments[idContent];
	mFragments.erase(idContent);
}

void SingleWireScheduling::fragmentDelete()
{
	uint8_t idContent = mResp.idContent;
	bool fragmentFound =
			mFragments.find(idContent) != mFragments.end();

	if (!fragmentFound)
		return;

	mFragments.erase(idContent);
}

void SingleWireScheduling::targetOnlineSet(bool online)
{
	mTargetIsOnline = online;

	if (mTargetIsOnlineOld == mTargetIsOnline)
		return;
	mTargetIsOnlineOld = online;

	if (online)
		return;

	if (mTargetIsOfflineMarked)
		return;
	mTargetIsOfflineMarked = true;

	mContentProc += "\r\n[Target is offline]\r\n";
	mContentProcChanged = true;
}

void SingleWireScheduling::responseReset(uint8_t idContent)
{
	mResp.idContent = idContent;
	mResp.content = "";
}

void SingleWireScheduling::processInfo(char *pBuf, char *pBufEnd)
{
	dInfo("Manual control\t\t%sabled\n", env.ctrlManual ? "En" : "Dis");
#if 1
	dInfo("State\t\t\t%s\n", ProcStateString[mState]);
#endif
#if 1
	dInfo("State SWT\t\t\t%s\n", SwtStateString[mStateSwt]);
#endif
	dInfo("Virtual UART mode\t\t%s\n", uartVirtualMode ? "uart" : "swart");
	dInfo("Virtual UART\t\t%sabled\n", uartVirtual ? "En" : "Dis");
	dInfo("UART: %s\t%sline\n",
			env.deviceUart.c_str(),
			mDevUartIsOnline ? "On" : "Off");
	dInfo("Target\t\t\t%sline\n", mTargetIsOnline ? "On" : "Off");
	dInfo("Bytes received\t\t%zu\n", mCntBytesRcvd);
	dInfo("IdContentNone received\t%zu\n", mCntContentNoneRcvd);
#if 0
	dInfo("Fragments\n");

	if (!mFragments.size())
	{
		dInfo("  None\n");
		return;
	}

	map<int, string>::iterator iter;

	iter = mFragments.begin();
	for (; iter != mFragments.end(); ++iter)
		dInfo("  %02X > '%s'\n", iter->first, iter->second.c_str());
#endif
#if 0
	dInfo("Command requests\n");
	dInfo("ID next\t\t\t%u\n", idReqCmdNext);

	list<CommandReqResp> *pList;
	list<CommandReqResp>::iterator iter;
	uint32_t curTimeMs = millis();
	uint32_t diffMs;

	for (size_t i = 0; i < 3; ++i)
	{
		pList = &requestsCmd[i];

		dInfo("Priority %zu\n", i);

		if (!pList->size())
		{
			dInfo("  None\n");
			continue;
		}

		iter = pList->begin();
		for (; iter != pList->end(); ++iter)
		{
			diffMs = curTimeMs - iter->startMs;

			if (diffMs > cTimeoutCmdReq)
				diffMs = cTimeoutCmdReq;

			dInfo("  Req %u: %s (%u)\n",
					iter->idReq,
					iter->str.c_str(),
					diffMs);
		}
	}

	dInfo("Command responses\n");
	iter = responsesCmd.begin();
	for (; iter != responsesCmd.end(); ++iter)
	{
		diffMs = curTimeMs - iter->startMs;

		if (diffMs > cTimeoutCmdReq)
			diffMs = cTimeoutCmdReq;

		dInfo("  Resp %u: %s (%u)\n",
				iter->idReq,
				iter->str.c_str(),
				diffMs);
	}

#endif
}

/* static functions */

bool SingleWireScheduling::commandSend(const string &cmd, uint32_t &idReq, PrioCmd prio)
{
	// optional mutex

	list<CommandReqResp> *pList = &requestsCmd[prio];

	if (pList->size() > cNumRequestsCmdMax)
		return false;

	if (responsesCmd.size() > cNumRequestsCmdMax)
		return false;

	idReq = idReqCmdNext;
	++idReqCmdNext;

	pList->emplace_back(cmd, idReq, millis());

	return true;
}

bool SingleWireScheduling::commandResponseGet(uint32_t idReq, string &resp)
{
	list<CommandReqResp>::iterator iter;

	iter = responsesCmd.begin();
	while (iter != responsesCmd.end())
	{
		if (iter->idReq != idReq)
		{
			++iter;
			continue;
		}

		resp = iter->str;
		iter = responsesCmd.erase(iter);

		return true;
	}

	return false;
}

void SingleWireScheduling::cmdCtrlManualToggle(char *pArgs, char *pBuf, char *pBufEnd)
{
	(void)pArgs;

	env.ctrlManual ^= 1;
	dInfo("Manual control %sabled", env.ctrlManual ? "en" : "dis");
}

void SingleWireScheduling::cmdDataUartSend(char *pArgs, char *pBuf, char *pBufEnd)
{
	if (!env.ctrlManual)
	{
		dInfo("Manual control disabled");
		return;
	}

	dataUartSend(pArgs, pBuf, pBufEnd, uartSend);
}

void SingleWireScheduling::cmdStrUartSend(char *pArgs, char *pBuf, char *pBufEnd)
{
	if (!env.ctrlManual)
	{
		dInfo("Manual control disabled");
		return;
	}

	strUartSend(pArgs, pBuf, pBufEnd, uartSend);
}

void SingleWireScheduling::cmdDataUartRead(char *pArgs, char *pBuf, char *pBufEnd)
{
	(void)pArgs;

	if (!env.ctrlManual)
	{
		dInfo("Manual control disabled");
		return;
	}

	ssize_t lenDone;
	char buf[55];

	lenDone = uartRead(refUart, buf, sizeof(buf));
	if (!lenDone)
	{
		dInfo("No data");
		return;
	}

	if (lenDone < 0)
	{
		dInfo("Error receiving data: %zu", lenDone);
		return;
	}

	ssize_t lenMax = 32;
	ssize_t lenReq = PMIN(lenMax, lenDone);

	hexDumpPrint(pBuf, pBufEnd, buf, lenReq, NULL, 8);
}

void SingleWireScheduling::cmdModeUartVirtSet(char *pArgs, char *pBuf, char *pBufEnd)
{
	if (pArgs && *pArgs == 'u')
		uartVirtualMode = 1;
	else
		uartVirtualMode = 0;

	dInfo("Virtual UART mode: %s", uartVirtualMode ? "uart" : "swart");
}

void SingleWireScheduling::cmdUartVirtToggle(char *pArgs, char *pBuf, char *pBufEnd)
{
	(void)pArgs;

	uartVirtual ^= 1;
	dInfo("Virtual UART %sabled", uartVirtual ? "en" : "dis");
}

void SingleWireScheduling::cmdMountedUartVirtToggle(char *pArgs, char *pBuf, char *pBufEnd)
{
	(void)pArgs;

	uartVirtualMounted ^= 1;
	dInfo("Virtual UART %smounted", uartVirtualMounted ? "" : "un");
}

void SingleWireScheduling::cmdTimeoutUartVirtToggle(char *pArgs, char *pBuf, char *pBufEnd)
{
	(void)pArgs;

	uartVirtualTimeout ^= 1;
	dInfo("Virtual UART timeout %s", uartVirtualTimeout ? "set" : "cleared");
}

void SingleWireScheduling::cmdDataUartRcv(char *pArgs, char *pBuf, char *pBufEnd)
{
	dataUartSend(pArgs, pBuf, pBufEnd, uartVirtRcv);
}

void SingleWireScheduling::cmdStrUartRcv(char *pArgs, char *pBuf, char *pBufEnd)
{
	strUartSend(pArgs, pBuf, pBufEnd, uartVirtRcv);
}

void SingleWireScheduling::dataUartSend(char *pArgs, char *pBuf, char *pBufEnd, FuncUartSend pFctSend)
{
	if (!pArgs)
	{
		dInfo("No data given");
		return;
	}

	string str = string(pArgs, strlen(pArgs));

	if (str == "flowOut")  str = "0B";
	if (str == "flowIn")   str = "0C";
	if (str == "cmdOut")   str = "1A";
	if (str == "none")     str = "15";
	if (str == "proc")     str = "11";
	if (str == "log")      str = "12";
	if (str == "cmd")      str = "13";
	if (str == "cut")      str = "0F";
	if (str == "end")      str = "17";
	if (str == "tab")      str = "09";
	if (str == "cr")       str = "0D";
	if (str == "lf")       str = "0A";

	vector<char> vData = toHex(str);
	pFctSend(refUart, vData.data(), vData.size());

	dInfo("Data moved");
}

void SingleWireScheduling::strUartSend(char *pArgs, char *pBuf, char *pBufEnd, FuncUartSend pFctSend)
{
	if (!pArgs)
	{
		dInfo("No string given");
		return;
	}

	pFctSend(refUart, pArgs, strlen(pArgs));
	dInfo("String moved");
}

// TEMP
void SingleWireScheduling::cmdCommandSend(char *pArgs, char *pBuf, char *pBufEnd)
{
	uint32_t idReq;
	bool ok;

	ok = SingleWireScheduling::commandSend(pArgs, idReq);
	if (!ok)
	{
		dInfo("Could not send command: %s", pArgs);
		return;
	}

	dInfo("Command sent: %s", pArgs);
}
// TEMP end

