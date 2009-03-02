//	-*- tab-width: 2; -*-

#ifndef TEXMODULE_h
#define TEXMODULE_h 1

#include <Carbon/Carbon.h>

#include "BBLMInterface.h"
#include "BBXTInterface.h"
#include "BBLMTextIterator.h"

void scan_tex(			BBLMParamBlock    &params,
							const BBLMCallbackBlock &callbacks,
							const BBXTCallbackBlock& /*bbxt_callbacks*/,
										bool							colorMathStrings);

void	ParseTeX(BBLMParamBlock    &bblm_params,
							 const BBLMCallbackBlock &bblm_callbacks);

OSErr	TeXModule(BBLMParamBlock &params,
								const BBLMCallbackBlock &bblm_callbacks,
								const BBXTCallbackBlock &bbxt_callbacks);

#endif	// TEXMODULE_h
