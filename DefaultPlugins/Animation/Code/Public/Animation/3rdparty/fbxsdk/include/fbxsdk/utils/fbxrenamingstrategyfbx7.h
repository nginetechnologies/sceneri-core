/****************************************************************************************
 
   Copyright (C) 2015 Autodesk, Inc.
   All rights reserved.
 
   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.
 
****************************************************************************************/

//! \file fbxrenamingstrategyfbx7.h
#ifndef _FBXSDK_UTILS_RENAMINGSTRATEGY_FBX7_H_
#define _FBXSDK_UTILS_RENAMINGSTRATEGY_FBX7_H_

#include <Animation/3rdparty/fbxsdk/include/fbxsdk/fbxsdk_def.h>

#include <Animation/3rdparty/fbxsdk/include/fbxsdk/utils/fbxrenamingstrategybase.h>

#include <Animation/3rdparty/fbxsdk/include/fbxsdk/fbxsdk_nsbegin.h>

class FBXSDK_DLL FbxRenamingStrategyFbx7 : public FbxRenamingStrategyBase
{
public:
    FbxRenamingStrategyFbx7();
    virtual ~FbxRenamingStrategyFbx7();

    void CleanUp() override;
    bool DecodeScene(FbxScene* pScene) override;
    bool EncodeScene(FbxScene* pScene) override;
    bool DecodeString(FbxNameHandler& pName) override;
    bool EncodeString(FbxNameHandler& pName, bool pIsPropertyName=false) override;
};

#include <Animation/3rdparty/fbxsdk/include/fbxsdk/fbxsdk_nsend.h>

#endif /* _FBXSDK_UTILS_RENAMINGSTRATEGY_FBX7_H_ */
