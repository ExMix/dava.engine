/*==================================================================================
    Copyright (c) 2008, binaryzebra
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    * Neither the name of the binaryzebra nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE binaryzebra AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL binaryzebra BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
=====================================================================================*/


#ifndef SCANSCALAR_H_62B23520_7C8E_11DE_8A39_0800200C9A66
#define SCANSCALAR_H_62B23520_7C8E_11DE_8A39_0800200C9A66

#if defined(_MSC_VER) || (defined(__GNUC__) && (__GNUC__ == 3 && __GNUC_MINOR__ >= 4) || (__GNUC__ >= 4)) // GCC supports "pragma once" correctly since 3.4
#pragma once
#endif


#include <string>
#include "regex.h"
#include "stream.h"

namespace YAML
{
	enum CHOMP { STRIP = -1, CLIP, KEEP };
	enum ACTION { NONE, BREAK, THROW };
	enum FOLD { DONT_FOLD, FOLD_BLOCK, FOLD_FLOW };

	struct ScanScalarParams {
		ScanScalarParams(): eatEnd(false), indent(0), detectIndent(false), eatLeadingWhitespace(0), escape(0), fold(DONT_FOLD),
			trimTrailingSpaces(0), chomp(CLIP), onDocIndicator(NONE), onTabInIndentation(NONE), leadingSpaces(false) {}

		// input:
		RegEx end;                      // what condition ends this scalar?
		bool eatEnd;                    // should we eat that condition when we see it?
		int indent;                     // what level of indentation should be eaten and ignored?
		bool detectIndent;              // should we try to autodetect the indent?
		bool eatLeadingWhitespace;      // should we continue eating this delicious indentation after 'indent' spaces?
		char escape;                    // what character do we escape on (i.e., slash or single quote) (0 for none)
		FOLD fold;                      // how do we fold line ends?
		bool trimTrailingSpaces;        // do we remove all trailing spaces (at the very end)
		CHOMP chomp;                    // do we strip, clip, or keep trailing newlines (at the very end)
		                                //   Note: strip means kill all, clip means keep at most one, keep means keep all
		ACTION onDocIndicator;          // what do we do if we see a document indicator?
		ACTION onTabInIndentation;      // what do we do if we see a tab where we should be seeing indentation spaces

		// output:
		bool leadingSpaces;
	};

	std::string ScanScalar(Stream& INPUT, ScanScalarParams& info);
}

#endif // SCANSCALAR_H_62B23520_7C8E_11DE_8A39_0800200C9A66

