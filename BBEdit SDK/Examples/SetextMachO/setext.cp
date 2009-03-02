#include <string.h>

#include <Carbon/Carbon.h>

#include "BBLMInterface.h"
#include "BBXTInterface.h"

#pragma mark Prototypes
#pragma mark -

OSErr	ScanForFunctions(BBLMParamBlock &params, const BBLMCallbackBlock *bblmCallbacks, const BBXTCallbackBlock *bbxtCallbacks);

#pragma mark Implementation
#pragma mark -

#define kMaxLineLength	256

static OSErr add_fold_range(const BBLMCallbackBlock *bblmCallbacks, BBLMProcInfo proc_info)
{
	OSStatus	status = noErr;
	SInt32		funcNameLength = proc_info.fSelEnd - proc_info.fFunctionStart;
	SInt32		foldRangeStart = proc_info.fFunctionStart + (2 * funcNameLength) + 2;
	SInt32		foldRangeLength = proc_info.fFunctionEnd - foldRangeStart - 1;
	
	if (foldRangeLength > 0)
		status = bblmAddFoldRange(bblmCallbacks, foldRangeStart, foldRangeLength, kBBLMGenericAutoFold);
		
	return status;
}

template<class T>
OSErr	scan_for_functions(T *text, BBLMParamBlock &params, const BBLMCallbackBlock *bblmCallbacks, const BBXTCallbackBlock *bbxtCallbacks)
{
	OSErr	result = noErr;
	UInt32	extent = params.fTextLength;
	UInt32	ix = 0;
	UInt32	count = 0;
	bool	has_title = false;
	T		*curChar = text;
	T		last_line[kMaxLineLength];
	T		cur_line[kMaxLineLength];
	SInt16	last_length 	= 0;
	SInt16	cur_length 		= 0;
	SInt32	last_start 		= 0;
	SInt32	cur_start 		= 0;
	
	last_line[last_length] 	= '\0';
	cur_line[cur_length]	= '\0';
	
	(void)bbxtCallbacks;	// unused
	
	while ( ( ix < extent ) && ( result == noErr ) )
	{
		cur_length = 0;
		cur_start = ix;
		
		while ( ( ix < extent ) && (*curChar != 0x0D) )
		{
			if ( cur_length < ( kMaxLineLength - 1 ) )
				cur_line[cur_length++] = *curChar;
			curChar++;
			ix++;
		}
		
		cur_line[cur_length] = 0;
		
		if ((cur_length > 0) && (cur_length == last_length))
		{
			//	possible section break
			int		i;
			T		match = ((cur_line[0] == '=') ? '=' : '-');
			
			for (i = 0; i < cur_length; i++)
			{
				if (cur_line[i] != match)
					break;
			}
			
			if ( ( count == 0 ) && ( match == '=' ) )
				has_title = true;
			
			//	if all of the characters were '-', then it's a subhead
			//	and we can add it.
			
			if (i == cur_length)
			{
				BBLMProcInfo	proc_info;
				UInt32			index = 0;
				
				memset(&proc_info, 0, sizeof(proc_info));
				
				proc_info.fFunctionStart = last_start;
				proc_info.fFunctionEnd = proc_info.fFunctionStart + last_length;
				proc_info.fSelStart = last_start;
				proc_info.fSelEnd = proc_info.fSelStart + last_length;
				proc_info.fFirstChar = last_start;
				
				if ( has_title && ( match == '-' ) )
					proc_info.fIndentLevel = 1;
					
				proc_info.fKind = kBBLMFunctionMark;
				proc_info.fFlags = 0;
				proc_info.fNameStart = 0;
				proc_info.fNameLength = last_length;

				result = bblmAddTokenToBuffer(bblmCallbacks, params.fFcnParams.fTokenBuffer,
												text + last_start, last_length,
												params.fTextIsUnicode, &proc_info.fNameStart);
												
				if ( result == noErr )
					result = bblmAddFunctionToList(bblmCallbacks, params.fFcnParams.fFcnList, proc_info, &index);
				count++;
					
				// fix up the previous function entry to get the
				// function range right.
				
				if ((index > 0) && (last_start > 0))
				{
					BBLMProcInfo	prev_proc;
					
					if ((bblmGetFunctionEntry(bblmCallbacks, params.fFcnParams.fFcnList, index - 1, prev_proc) == noErr) &&
						((! has_title) || (prev_proc.fIndentLevel > 0)))
					{
						prev_proc.fFunctionEnd = last_start - 1;
						
						bblmUpdateFunctionEntry(bblmCallbacks, params.fFcnParams.fFcnList, index - 1, prev_proc);
						add_fold_range(bblmCallbacks, prev_proc);
					}
				}
			}
		}
		
		BlockMoveData(cur_line, last_line, cur_length + 1);
		last_length = cur_length;
		last_start = cur_start;
		
		curChar++;
		ix++;
	}
	
	//	tie off the last function entry.
	
	if (count > 0)
	{
		BBLMProcInfo	prev_proc;
		
		if ((bblmGetFunctionEntry(bblmCallbacks, params.fFcnParams.fFcnList, count - 1, prev_proc) == noErr) &&
			((! has_title) || (prev_proc.fIndentLevel > 0)))
		{
			prev_proc.fFunctionEnd = extent;
			
			bblmUpdateFunctionEntry(bblmCallbacks, params.fFcnParams.fFcnList, count - 1, prev_proc);
			add_fold_range(bblmCallbacks, prev_proc);
		}
	}
	
	return result;
}

OSErr	ScanForFunctions(BBLMParamBlock &params, const BBLMCallbackBlock *bblmCallbacks, const BBXTCallbackBlock *bbxtCallbacks)
{
	OSErr	result = noErr;
	
	(void)bbxtCallbacks;	// unused

	if ( params.fTextGapLength != 0 )
		result = paramErr;				// we assume that this is zero because the app sets it so
										// for function scanner calls

	if ( result == noErr )
		result = bblmResetTokenBuffer(bblmCallbacks, params.fFcnParams.fTokenBuffer);
	
	if ( result == noErr )
		result = bblmResetProcList(bblmCallbacks, params.fFcnParams.fFcnList);
		
	if ( result == noErr )
	{
		if ( params.fTextIsUnicode )
		{
			result = scan_for_functions((UInt16*)params.fText, params, bblmCallbacks, bbxtCallbacks);
		}
		else
		{
			result = scan_for_functions((UInt8*)params.fText, params, bblmCallbacks, bbxtCallbacks);
		}
	}
	
	return result;
}

#pragma mark -

extern "C"
{

OSErr	SetextMachO(BBLMParamBlock &params, const BBLMCallbackBlock &bblmCallbacks, const BBXTCallbackBlock &bbxtCallbacks)
{
	OSErr	result = noErr;
	
	//
	//	a language module must always make sure that the parameter block
	//	is valid by checking the signature, version number, and size
	//	of the parameter block. Note also that version 2 is the first
	//	usable version of the parameter block; anything older should
	//	be rejected.
	//
	
	//
	//	RMS 010925 the check for params.fVersion > kBBLMParamBlockVersion
	//	is overly strict, since there are no structural changes that would
	//	break backward compatibility; only new members are added.
	//

	if ((params.fSignature != kBBLMParamBlockSignature) ||
		(params.fVersion == 0) ||
		(params.fVersion < 2) ||
		(params.fLength < sizeof(BBLMParamBlock)))
	{
		return paramErr;
	}
	
	switch (params.fMessage)
	{
		case kBBLMInitMessage:
		case kBBLMDisposeMessage:
		{
			result = noErr;	// nothing to do
			break;
		}
		
		case kBBLMScanForFunctionsMessage:
		{
			result = ScanForFunctions(params, &bblmCallbacks, &bbxtCallbacks);
			break;
		}
		
		case kBBLMAdjustRangeMessage:
		case kBBLMCalculateRunsMessage:
		case kBBLMAdjustEndMessage:
		case kBBLMMapColorCodeToColorMessage:
		case kBBLMMapRunKindToColorCodeMessage:
		case kBBLMSetCategoriesMessage:
		case kBBLMMatchKeywordMessage:
		case kBBLMEscapeStringMessage:
		{
			result = userCanceledErr;
			break;
		}
			
		default:
		{
			result = paramErr;
			break;
		}
	}
	
	return result;
}
}
