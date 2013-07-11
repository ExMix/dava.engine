#ifndef __FRAME_PATH_HELPER_H__
#define __FRAME_PATH_HELPER_H__

#include "Base/BaseTypes.h"
#include "FileSystem/FilePath.h"

namespace DAVA {

class FramePathHelper
{
public:
	// Convert the frame name and index to relative PNG frame path.
	static String GetFramePathRelative(const String& nameWithoutExt, int32 frameIndex);
	
	// The same but with absolute path.
	static FilePath GetFramePathAbsolute(const FilePath& directory, const String& nameWithoutExt, int32 frameIndex);
};

};

#endif //__FRAME_PATH_HELPER_H__
