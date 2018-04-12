//***************************************************************************************
// GameTimer.cpp by Frank Luna (C) 2011 All Rights Reserved.
//***************************************************************************************

#include <windows.h>
#include "GameTimer.h"

GameTimer::GameTimer()
: mSecondsPerCount(0.0), mDeltaTime(-1.0), mBaseTime(0), 
  mPausedTime(0), mPrevTime(0), mCurrTime(0), mStopped(false)
{
	__int64 countsPerSec;
	QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);
	mSecondsPerCount = 1.0 / (double)countsPerSec;
}

// Reset�� ȣ��� ���� �帥 �ð����� �Ͻ� ������ �ð��� ������ �ð��� ��ȯ
// Returns the total time elapsed since Reset() was called, 
// NOT counting any time when the clock is stopped.
float GameTimer::TotalTime()const
{
	// Ÿ�̸Ӱ� ���������̸�, ������ �������� �帥 �ð��� ����ϸ� �ȵȴ�.
	// ���� ������ �̹� �Ͻ� ������ ���� �ִٸ� �ð��� mStopTime - mBaseTime����
	// �Ͻ� ���� ���� �ð��� ���ԵǾ� �ִµ�, �� ���� �ð��� ��ü �ð��� �������� ���ƾ� �Ѵ�.
	// �̸� �ٷ���� ����, mStopTime���� �Ͻ� ���� ���� �ð��� ����.

	// If we are stopped, do not count the time that has passed since we stopped.
	// Moreover, if we previously already had a pause, the distance 
	// mStopTime - mBaseTime includes paused time, which we do not want to count.
	// To correct this, we can subtract the paused time from mStopTime:  
	//
	//                     |<--paused time-->|
	// ----*---------------*-----------------*------------*------------*------> time
	//  mBaseTime       mStopTime        startTime     mStopTime    mCurrTime

	if( mStopped )
	{
		return (float)(((mStopTime - mPausedTime)-mBaseTime)*mSecondsPerCount);
	}

	// ���� ����
	// ���� ���°� �ƴϱ� ������ ���� �ð��� �������� ����Ѵ�.
	// The distance mCurrTime - mBaseTime includes paused time,
	// which we do not want to count.  To correct this, we can subtract 
	// the paused time from mCurrTime:  
	//
	//  (mCurrTime - mPausedTime) - mBaseTime 
	//
	//                     |<--paused time-->|
	// ----*---------------*-----------------*------------*------> time
	//  mBaseTime       mStopTime        startTime     mCurrTime
	
	else
	{
		return (float)(((mCurrTime-mPausedTime)-mBaseTime)*mSecondsPerCount);
	}
}

float GameTimer::DeltaTime()const
{
	return (float)mDeltaTime;
}

void GameTimer::Reset()
{
	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

	mBaseTime = currTime;
	mPrevTime = currTime;
	mStopTime = 0;
	mStopped  = false;
}

void GameTimer::Start()
{
	__int64 startTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&startTime);


	// ����(�Ͻ�����)�� ����(�簳) ���̿� �帥 �ð��� �����Ѵ�.
	// Accumulate the time elapsed between stop and start pairs.
	//
	//                     |<-------d------->|
	// ----*---------------*-----------------*------------> time
	//  mBaseTime       mStopTime        startTime     

	// ���� ���¿��� Ÿ�̸Ӹ� �簳�ϴ� ���:
	if( mStopped )
	{
		// �Ͻ� ������ �ð��� �����Ѵ�.
		mPausedTime += (startTime - mStopTime);	

		// Ÿ�̸Ӹ� �ٽ� �����ϴ� ���̹Ƿ�, 
		// ������ mPrevTime(���� �ð�)�� ��ȿ���� �ʴ�.
		// ���� ���� �ð����� �ٽ� �����Ѵ�.
		mPrevTime = startTime;

		// ������ ���� ���°� �ƴϹǷ� ���� ������� �����Ѵ�.
		mStopTime = 0;
		mStopped  = false;
	}
}

void GameTimer::Stop()
{
	// �̹� ���� �����̸� �ƹ� �ϵ� ���� �ʴ´�.
	if( !mStopped )
	{
		__int64 currTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

		// ������ ó�� ȣ��Ǿ��ٸ�
		// Ÿ�̸� ���� �ð��� ���� �ð��� �Ҵ��Ѵ�.
		// Ÿ�̸Ӱ� �����Ǿ����� ���ϴ� bool flag�� true �� �Ҵ��Ѵ�.
		mStopTime = currTime;
		mStopped  = true;
	}
}

void GameTimer::Tick()
{
	if( mStopped )
	{
		mDeltaTime = 0.0;
		return;
	}

	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
	mCurrTime = currTime;

	// Time difference between this frame and the previous.
	mDeltaTime = (mCurrTime - mPrevTime)*mSecondsPerCount;

	// Prepare for next frame.
	mPrevTime = mCurrTime;

	// Force nonnegative.  The DXSDK's CDXUTTimer mentions that if the 
	// processor goes into a power save mode or we get shuffled to another
	// processor, then mDeltaTime can be negative.
	if(mDeltaTime < 0.0)
	{
		mDeltaTime = 0.0;
	}
}

