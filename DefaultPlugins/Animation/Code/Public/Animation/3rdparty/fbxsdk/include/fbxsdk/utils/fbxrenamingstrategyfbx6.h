/****************************************************************************************
 
   Copyright (C) 2015 Autodesk, Inc.
   All rights reserved.
 
   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.
 
****************************************************************************************/

//! \file fbxrenamingstrategyfbx6.h
#ifndef _FBXSDK_UTILS_RENAMINGSTRATEGY_FBX6_H_
#define _FBXSDK_UTILS_RENAMINGSTRATEGY_FBX6_H_

#include <Animation/3rdparty/fbxsdk/include/fbxsdk/fbxsdk_def.h>

#include <Animation/3rdparty/fbxsdk/include/fbxsdk/utils/fbxrenamingstrategybase.h>

#include <Animation/3rdparty/fbxsdk/include/fbxsdk/fbxsdk_nsbegin.h>

/** \brief This class contains the description of the FBX renaming strategy for fbx file format 6.
* \nosubgrouping
*/
class FBXSDK_DLL FbxRenamingStrategyFbx6: public FbxRenamingStrategyBase
{
public:

    //! Default constructor
    FbxRenamingStrategyFbx6();

    //! Destructor
    virtual ~FbxRenamingStrategyFbx6();

    /** This method put all the names in the scene back to the original values
    * \param pScene
    * \return Returns true if some names have been modified.
    */
    bool DecodeScene(FbxScene* pScene) override;

    /** This method renames all the names in the scene
    * \param pScene
    * \return Returns true if some names have been modified.
    */
    bool EncodeScene(FbxScene* pScene) override;

    /** This method find the original name of a given string
    * \param pName
    * \return Returns true if the name has been modified.
    */
    bool DecodeString(FbxNameHandler& pName) override;

    /** This method find the renaming name of a given string
    * \param pName
    * \param pIsPropertyName
    * \return Returns true if the name has been modified.
    */
    bool EncodeString(FbxNameHandler& pName, bool pIsPropertyName=false) override;

    //! clean up the name cells.
    void CleanUp() override;
};

#include <Animation/3rdparty/fbxsdk/include/fbxsdk/fbxsdk_nsend.h>

#endif /* _FBXSDK_UTILS_RENAMINGSTRATEGY_FBX6_H_ */
