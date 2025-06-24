# Copyright (C) 2024 Signal Slot Inc.
# SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#####################################################################
## PsdCore Module:
#####################################################################
qt_internal_add_module(PsdCore
    PLUGIN_TYPES psdadditionallayerinformation psddescriptor psdeffectslayer
    SOURCES
        qpsdabstractimage.cpp qpsdabstractimage.h
        qpsdadditionallayerinformation.cpp qpsdadditionallayerinformation.h
        qpsdchannelimagedata.cpp qpsdchannelimagedata.h
        qpsdchannelinfo.cpp qpsdchannelinfo.h
        qpsdcolormodedata.cpp qpsdcolormodedata.h
        qpsdcolorspace.cpp qpsdcolorspace.h
        qpsddescriptor.cpp qpsddescriptor.h
        qpsdfileheader.cpp qpsdfileheader.h
        qpsdgloballayermaskinfo.cpp qpsdgloballayermaskinfo.h
        qpsdimagedata.cpp qpsdimagedata.h
        qpsdimageresourceblock.cpp qpsdimageresourceblock.h
        qpsdimageresources.cpp qpsdimageresources.h
        qpsdresolutioninfo.cpp qpsdresolutioninfo.h
        qpsdlayerandmaskinformation.cpp qpsdlayerandmaskinformation.h
        qpsdlayerblendingrangesdata.cpp qpsdlayerblendingrangesdata.h
        qpsdlayerinfo.cpp qpsdlayerinfo.h
        qpsdlayermaskadjustmentlayerdata.cpp qpsdlayermaskadjustmentlayerdata.h
        qpsdlayerrecord.cpp qpsdlayerrecord.h
        qpsdparser.cpp qpsdparser.h
        qpsdsection.cpp qpsdsection.h
        qpsdeffectslayer.h qpsdeffectslayer.cpp
        qpsdsectiondividersetting.h qpsdsectiondividersetting.cpp
        qpsdadditionallayerinformationplugin.h qpsdadditionallayerinformationplugin.cpp
        qpsddescriptorplugin.h qpsddescriptorplugin.cpp
        qpsdabstractplugin.h qpsdabstractplugin.cpp
        qpsdmetadataitem.h qpsdmetadataitem.cpp
        qpsdenginedataparser.h qpsdenginedataparser.cpp
        qpsdtypetoolobjectsetting.h qpsdtypetoolobjectsetting.cpp
        qpsdeffectslayerplugin.h qpsdeffectslayerplugin.cpp
        qpsdenum.h qpsdenum.cpp
        qpsdsofieffect.h qpsdsofieffect.cpp
        qpsdabstracteffect.h qpsdabstracteffect.cpp
        qpsdoglweffect.h qpsdoglweffect.cpp
        qpsdiglweffect.h qpsdiglweffect.cpp
        qpsdshadoweffect.h qpsdshadoweffect.cpp
        qpsdbevleffect.h qpsdbevleffect.cpp
        qpsdvectorstrokedata.h qpsdvectorstrokedata.cpp
        qpsdvectormasksetting.h qpsdvectormasksetting.cpp
        qpsdunitfloat.h qpsdunitfloat.cpp
        qpsdlinkedlayer.h qpsdlinkedlayer.cpp
        qpsdplacedlayer.h qpsdplacedlayer.cpp
        qpsdplacedlayerdata.h qpsdplacedlayerdata.cpp
        qpsdblend.h qpsdblend.cpp
        qpsdcoreglobal.h
        qpsdvectorstrokecontentsetting.h qpsdvectorstrokecontentsetting.cpp
        qpsdlayertreeitemmodel.h qpsdlayertreeitemmodel.cpp
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}
    LIBRARIES
        Qt::CorePrivate
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::Gui
    PRIVATE_MODULE_INTERFACE
        Qt::CorePrivate
)

## Scopes:
#####################################################################

qt_internal_extend_target(PsdCore CONDITION WIN32
    SOURCES
        qpsdabstractplugin_win.cpp
)

qt_internal_extend_target(PsdCore CONDITION LINUX
    SOURCES
        qpsdabstractplugin_linux.cpp
)

qt_internal_extend_target(PsdCore CONDITION APPLE
    SOURCES
        qpsdabstractplugin_mac.cpp
)
