# Many warnings caused by vtk
if(CMAKE_COMPILER_IS_CLANGXX)
    add_compile_options(-Wno-pedantic) # needed for vtk headers

    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 4.0)
        add_compile_options(-Wno-inconsistent-missing-override)
    endif()
elseif(CMAKE_COMPILER_IS_GNUCXX)
    add_compile_options(-Wno-pedantic) # needed for vtk headers
endif()

if(BUILD_FEM_NETGEN)
    add_definitions(-DFCWithNetgen)
endif(BUILD_FEM_NETGEN)

include_directories(
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)

include_directories(
    SYSTEM
    ${SMESH_INCLUDE_DIR}
    ${VTK_INCLUDE_DIRS}
)

link_directories(${SMESH_LIB_PATH})

set(FemGui_LIBS
    Fem
    FreeCADGui
    PartGui
)

generate_from_xml(ViewProviderFemConstraintPy)
generate_from_xml(ViewProviderFemMeshPy)
generate_from_xml(ViewProviderFemPostPipelinePy)
generate_from_xml(ViewProviderFemPostFilterPy)

SET(Python_SRCS
    ViewProviderFemConstraintPy.xml
    ViewProviderFemConstraintPyImp.cpp
    ViewProviderFemMeshPy.xml
    ViewProviderFemMeshPyImp.cpp
)
if(BUILD_FEM_VTK)
    set(Python_SRCS
        ${Python_SRCS}
        ViewProviderFemPostPipelinePy.xml
        ViewProviderFemPostPipelinePyImp.cpp
        ViewProviderFemPostFilterPy.xml
        ViewProviderFemPostFilterPyImp.cpp
    )
endif(BUILD_FEM_VTK)

SOURCE_GROUP("Python" FILES ${Python_SRCS})


set(FemGui_UIC_SRCS
    DlgSettingsFemCcx.ui
    DlgSettingsFemElmer.ui
    DlgSettingsFemExportAbaqus.ui
    DlgSettingsFemGeneral.ui
    DlgSettingsFemGmsh.ui
    DlgSettingsFemInOutVtk.ui
    DlgSettingsFemMystran.ui
    DlgSettingsFemZ88.ui
    TaskCreateElementSet.ui
    TaskCreateNodeSet.ui
    TaskObjectName.ui
    TaskFemConstraint.ui
    TaskFemConstraintBearing.ui
    TaskFemConstraintFixed.ui
    TaskFemConstraintRigidBody.ui
    TaskFemConstraintForce.ui
    TaskFemConstraintFluidBoundary.ui
    TaskFemConstraintPressure.ui
    TaskFemConstraintDisplacement.ui
    TaskFemConstraintTemperature.ui
    TaskFemConstraintHeatflux.ui
    TaskFemConstraintInitialTemperature.ui
    TaskFemConstraintPlaneRotation.ui
    TaskFemConstraintContact.ui
    TaskFemConstraintTransform.ui
    TaskFemConstraintSpring.ui
    TaskTetParameter.ui
    TaskAnalysisInfo.ui
    TaskDriver.ui
)

if(BUILD_FEM_VTK)
    set(FemGui_UIC_SRCS
        ${FemGui_UIC_SRCS}
        BoxWidget.ui
        CylinderWidget.ui
        PlaneWidget.ui
        SphereWidget.ui
        TaskPostCalculator.ui
        TaskPostClip.ui
        TaskPostContours.ui
        TaskPostCut.ui
        TaskPostDataAlongLine.ui
        TaskPostDataAtPoint.ui
        TaskPostDisplay.ui
        TaskPostScalarClip.ui
        TaskPostWarpVector.ui
        TaskPostFrames.ui
        TaskPostBranch.ui
    )
endif(BUILD_FEM_VTK)

SET(FemGui_DLG_SRCS
    ${FemGui_UIC_HDRS}
    DlgSettingsFemCcx.ui
    DlgSettingsFemCcxImp.cpp
    DlgSettingsFemCcxImp.h
    DlgSettingsFemElmer.ui
    DlgSettingsFemElmerImp.cpp
    DlgSettingsFemElmerImp.h
    DlgSettingsFemExportAbaqus.ui
    DlgSettingsFemExportAbaqusImp.cpp
    DlgSettingsFemExportAbaqusImp.h
    DlgSettingsFemGeneral.ui
    DlgSettingsFemGeneralImp.cpp
    DlgSettingsFemGeneralImp.h
    DlgSettingsFemGmsh.ui
    DlgSettingsFemGmshImp.cpp
    DlgSettingsFemGmshImp.h
    DlgSettingsFemInOutVtk.ui
    DlgSettingsFemInOutVtkImp.cpp
    DlgSettingsFemInOutVtkImp.h
    DlgSettingsFemMystran.ui
    DlgSettingsFemMystranImp.cpp
    DlgSettingsFemMystranImp.h
    DlgSettingsFemZ88.ui
    DlgSettingsFemZ88Imp.cpp
    DlgSettingsFemZ88Imp.h
    TaskFemConstraint.ui
    TaskFemConstraint.cpp
    TaskFemConstraint.h
    TaskFemConstraintOnBoundary.cpp
    TaskFemConstraintOnBoundary.h
    TaskFemConstraintBearing.ui
    TaskFemConstraintBearing.cpp
    TaskFemConstraintBearing.h
    TaskFemConstraintFixed.ui
    TaskFemConstraintFixed.cpp
    TaskFemConstraintFixed.h
    TaskFemConstraintRigidBody.ui
    TaskFemConstraintRigidBody.cpp
    TaskFemConstraintRigidBody.h
    TaskFemConstraintForce.ui
    TaskFemConstraintForce.cpp
    TaskFemConstraintForce.h
    TaskFemConstraintFluidBoundary.ui
    TaskFemConstraintFluidBoundary.cpp
    TaskFemConstraintFluidBoundary.h
    TaskFemConstraintPressure.ui
    TaskFemConstraintPressure.cpp
    TaskFemConstraintPressure.h
    TaskFemConstraintSpring.ui
    TaskFemConstraintSpring.cpp
    TaskFemConstraintSpring.h
    TaskFemConstraintGear.cpp
    TaskFemConstraintGear.h
    TaskFemConstraintPulley.cpp
    TaskFemConstraintPulley.h
    TaskFemConstraintDisplacement.ui
    TaskFemConstraintDisplacement.cpp
    TaskFemConstraintDisplacement.h
    TaskFemConstraintTemperature.ui
    TaskFemConstraintTemperature.cpp
    TaskFemConstraintTemperature.h
    TaskFemConstraintHeatflux.ui
    TaskFemConstraintHeatflux.cpp
    TaskFemConstraintHeatflux.h
    TaskFemConstraintInitialTemperature.ui
    TaskFemConstraintInitialTemperature.cpp
    TaskFemConstraintInitialTemperature.h
    TaskFemConstraintPlaneRotation.ui
    TaskFemConstraintPlaneRotation.cpp
    TaskFemConstraintPlaneRotation.h
    TaskFemConstraintContact.ui
    TaskFemConstraintContact.cpp
    TaskFemConstraintContact.h
    TaskFemConstraintTransform.ui
    TaskFemConstraintTransform.cpp
    TaskFemConstraintTransform.h
)
SOURCE_GROUP("Constraint-Dialogs" FILES ${FemGui_DLG_SRCS})

set(Fem_TR_QRC ${CMAKE_CURRENT_BINARY_DIR}/Resources/Fem_translation.qrc)
qt_find_and_add_translation(QM_SRCS "Resources/translations/*_*.ts"
    ${CMAKE_CURRENT_BINARY_DIR}/Resources/translations)
qt_create_resource_file(${Fem_TR_QRC} ${QM_SRCS})
qt_add_resources(FemResource_SRCS Resources/Fem.qrc ${Fem_TR_QRC})
SOURCE_GROUP("Resources" FILES ${FemResource_SRCS})

SET(FemGui_SRCS_ViewProvider
    ViewProviderFemMesh.cpp
    ViewProviderFemMesh.h
    ViewProviderFemMeshShape.cpp
    ViewProviderFemMeshShape.h
    ViewProviderFemMeshShapeNetgen.cpp
    ViewProviderFemMeshShapeNetgen.h
    ViewProviderAnalysis.cpp
    ViewProviderAnalysis.h
    ViewProviderSolver.cpp
    ViewProviderSolver.h
    ViewProviderSetNodes.cpp
    ViewProviderSetNodes.h
    ViewProviderSetElementNodes.cpp
    ViewProviderSetElementNodes.h
    ViewProviderSetElements.cpp
    ViewProviderSetElements.h
    ViewProviderSetFaces.cpp
    ViewProviderSetFaces.h
    ViewProviderSetGeometry.cpp
    ViewProviderSetGeometry.h
    FemSelectionGate.cpp
    FemSelectionGate.h
    ViewProviderFemConstraint.cpp
    ViewProviderFemConstraint.h
    ViewProviderFemConstraintOnBoundary.cpp
    ViewProviderFemConstraintOnBoundary.h
    ViewProviderFemConstraintBearing.cpp
    ViewProviderFemConstraintBearing.h
    ViewProviderFemConstraintFixed.cpp
    ViewProviderFemConstraintFixed.h
    ViewProviderFemConstraintRigidBody.cpp
    ViewProviderFemConstraintRigidBody.h
    ViewProviderFemConstraintForce.cpp
    ViewProviderFemConstraintForce.h
    ViewProviderFemConstraintFluidBoundary.cpp
    ViewProviderFemConstraintFluidBoundary.h
    ViewProviderFemConstraintPressure.cpp
    ViewProviderFemConstraintPressure.h
    ViewProviderFemConstraintSpring.cpp
    ViewProviderFemConstraintSpring.h
    ViewProviderFemConstraintGear.cpp
    ViewProviderFemConstraintGear.h
    ViewProviderFemConstraintPulley.cpp
    ViewProviderFemConstraintPulley.h
    ViewProviderFemConstraintDisplacement.cpp
    ViewProviderFemConstraintDisplacement.h
    ViewProviderFemConstraintTemperature.cpp
    ViewProviderFemConstraintTemperature.h
    ViewProviderFemConstraintHeatflux.cpp
    ViewProviderFemConstraintHeatflux.h
    ViewProviderFemConstraintInitialTemperature.cpp
    ViewProviderFemConstraintInitialTemperature.h
    ViewProviderFemConstraintPlaneRotation.cpp
    ViewProviderFemConstraintPlaneRotation.h
    ViewProviderFemConstraintContact.cpp
    ViewProviderFemConstraintContact.h
    ViewProviderFemConstraintTransform.cpp
    ViewProviderFemConstraintTransform.h
    ViewProviderResult.cpp
    ViewProviderResult.h
)
SOURCE_GROUP("ViewProvider" FILES ${FemGui_SRCS_ViewProvider})

SET(FemGui_SRCS_TaskBoxes
    TaskObjectName.ui
    TaskObjectName.cpp
    TaskObjectName.h
    TaskCreateElementSet.ui
    TaskCreateElementSet.cpp
    TaskCreateElementSet.h
    TaskCreateNodeSet.ui
    TaskCreateNodeSet.cpp
    TaskCreateNodeSet.h
    TaskDriver.ui
    TaskDriver.cpp
    TaskDriver.h
    TaskAnalysisInfo.ui
    TaskAnalysisInfo.cpp
    TaskAnalysisInfo.h
    TaskTetParameter.ui
    TaskTetParameter.cpp
    TaskTetParameter.h
)

if(BUILD_FEM_VTK)
    SET(FemGui_SRCS_TaskBoxes
        ${FemGui_SRCS_TaskBoxes}
        BoxWidget.ui
        CylinderWidget.ui
        PlaneWidget.ui
        SphereWidget.ui
        TaskPostBoxes.h
        TaskPostBoxes.cpp
        TaskPostExtraction.h
        TaskPostExtraction.cpp
        TaskPostCalculator.ui
        TaskPostClip.ui
        TaskPostContours.ui
        TaskPostCut.ui
        TaskPostDataAlongLine.ui
        TaskPostDataAtPoint.ui
        TaskPostDisplay.ui
        TaskPostScalarClip.ui
        TaskPostWarpVector.ui
    )
endif(BUILD_FEM_VTK)

SOURCE_GROUP("Task_Boxes" FILES ${FemGui_SRCS_TaskBoxes})

SET(FemGui_SRCS_TaskDlg
    TaskDlgCreateElementSet.h
    TaskDlgCreateElementSet.cpp
    TaskDlgCreateNodeSet.h
    TaskDlgCreateNodeSet.cpp
    TaskDlgMeshShapeNetgen.h
    TaskDlgMeshShapeNetgen.cpp
    TaskDlgAnalysis.h
    TaskDlgAnalysis.cpp
)
SOURCE_GROUP("Task_Dialogs" FILES ${FemGui_SRCS_TaskDlg})

SET(FemGui_SRCS_Module
    AppFemGui.cpp
    AppFemGuiPy.cpp
    AbaqusHighlighter.cpp
    AbaqusHighlighter.h
    ActiveAnalysisObserver.cpp
    ActiveAnalysisObserver.h
    Command.cpp
    FemGuiTools.cpp
    FemGuiTools.h
    FemSettings.cpp
    FemSettings.h
    Resources/Fem.qrc
    PreCompiled.cpp
    PreCompiled.h
    PropertyFemMeshItem.cpp
    PropertyFemMeshItem.h
    Workbench.cpp
    Workbench.h
)
SOURCE_GROUP("Module" FILES ${FemGui_SRCS_Module})


if(BUILD_FEM_VTK)
    SET(FemGui_SRCS_Post
        ViewProviderFemPostObject.h
        ViewProviderFemPostObject.cpp
        ViewProviderFemPostPipeline.h
        ViewProviderFemPostPipeline.cpp
        ViewProviderFemPostBranchFilter.h
        ViewProviderFemPostBranchFilter.cpp
        ViewProviderFemPostFunction.h
        ViewProviderFemPostFunction.cpp
        ViewProviderFemPostFilter.h
        ViewProviderFemPostFilter.cpp
    )
    SOURCE_GROUP("PostObjects" FILES ${FemGui_SRCS_Post})
endif(BUILD_FEM_VTK)

SET(FemGui_SRCS
    ${Python_SRCS}
    ${FemGui_DLG_SRCS}
    ${FemResource_SRCS}
    ${FemGui_SRCS_ViewProvider}
    ${FemGui_SRCS_TaskDlg}
    ${FemGui_SRCS_TaskBoxes}
    ${FemGui_SRCS_Module}
    ${FemGui_SRCS_Post}
)

if(FREECAD_USE_PCH)
    add_definitions(-D_PreComp_)
    GET_MSVC_PRECOMPILED_SOURCE("PreCompiled.cpp" PCH_SRCS ${FemGui_SRCS})
    ADD_MSVC_PRECOMPILED_HEADER(FemGui PreCompiled.h PreCompiled.cpp PCH_SRCS)
endif(FREECAD_USE_PCH)

SET(FemGuiIcon_SVG
    Resources/icons/FemWorkbench.svg
)

SET(FemGuiSymbol_IV
    Resources/symbols/ConstraintContact.iv
    Resources/symbols/ConstraintDisplacement.iv
    Resources/symbols/ConstraintElectricChargeDensity.iv
    Resources/symbols/ConstraintElectrostaticPotential.iv
    Resources/symbols/ConstraintCurrentDensity.iv
    Resources/symbols/ConstraintFixed.iv
    Resources/symbols/ConstraintForce.iv
    Resources/symbols/ConstraintHeatFlux.iv
    Resources/symbols/ConstraintPlaneRotation.iv
    Resources/symbols/ConstraintPressure.iv
    Resources/symbols/ConstraintRigidBody.iv
    Resources/symbols/ConstraintSectionPrint.iv
    Resources/symbols/ConstraintSpring.iv
    Resources/symbols/ConstraintTemperature.iv
    Resources/symbols/ConstraintTransform.iv
    Resources/symbols/ConstraintTie.iv
)

add_library(FemGui SHARED ${FemGui_SRCS} ${FemGuiIcon_SVG} ${FemGuiSymbol_IV})
target_link_libraries(FemGui ${FemGui_LIBS} ${VTK_LIBRARIES})
if (FREECAD_WARN_ERROR)
    target_compile_warn_error(FemGui)
endif()

fc_copy_sources(FemGui "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_DATADIR}/Mod/Fem" ${FemGuiIcon_SVG})
fc_copy_sources(FemGui "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_DATADIR}/Mod/Fem" ${FemGuiSymbol_IV})

INSTALL(FILES ${FemGuiIcon_SVG} DESTINATION "${CMAKE_INSTALL_DATADIR}/Mod/Fem/Resources/icons")
INSTALL(FILES ${FemGuiSymbol_IV} DESTINATION "${CMAKE_INSTALL_DATADIR}/Mod/Fem/Resources/symbols")

# Python modules ui files, they are copied as they are, thus the need not to be added to Fem.qrc
# see https://forum.freecad.org/viewtopic.php?f=10&t=25833
SET(FemGuiPythonUI_SRCS
    Resources/ui/BodyHeatSource.ui
    Resources/ui/ConstraintCentrif.ui
    Resources/ui/ConstraintTie.ui
    Resources/ui/ConstraintSectionPrint.ui
    Resources/ui/CurrentDensity.ui
    Resources/ui/DlgSettingsNetgen.ui
    Resources/ui/ElectricChargeDensity.ui
    Resources/ui/ElectrostaticPotential.ui
    Resources/ui/ElementFluid1D.ui
    Resources/ui/ElementGeometry1D.ui
    Resources/ui/ElementGeometry2D.ui
    Resources/ui/ElementRotation1D.ui
    Resources/ui/FlowVelocity.ui
    Resources/ui/InitialFlowVelocity.ui
    Resources/ui/InitialPressure.ui
    Resources/ui/Magnetization.ui
    Resources/ui/Material.ui
    Resources/ui/MaterialReinforcement.ui
    Resources/ui/MeshBoundaryLayer.ui
    Resources/ui/MeshGmsh.ui
    Resources/ui/MeshGroup.ui
    Resources/ui/MeshGroupXDMFExport.ui
    Resources/ui/MeshNetgen.ui
    Resources/ui/MeshRegion.ui
    Resources/ui/ResultHints.ui
    Resources/ui/ResultShow.ui
    Resources/ui/SolverCalculiX.ui
    Resources/ui/SolverCcxTools.ui
    Resources/ui/TaskPostGlyph.ui
    Resources/ui/TaskPostExtraction.ui
    Resources/ui/TaskPostHistogram.ui
    Resources/ui/TaskPostLineplot.ui
    Resources/ui/PostHistogramFieldViewEdit.ui
    Resources/ui/PostHistogramFieldAppEdit.ui
    Resources/ui/PostHistogramIndexAppEdit.ui
    Resources/ui/PostLineplotFieldViewEdit.ui
    Resources/ui/PostLineplotFieldAppEdit.ui
    Resources/ui/PostLineplotIndexAppEdit.ui
    Resources/ui/PostTableFieldViewEdit.ui
)

ADD_CUSTOM_TARGET(FemPythonUi ALL
    SOURCES ${FemGuiPythonUI_SRCS}
)

fc_copy_sources(FemPythonUi "${CMAKE_BINARY_DIR}/Mod/Fem" ${FemGuiPythonUI_SRCS})

INSTALL(FILES ${FemGuiPythonUI_SRCS} DESTINATION Mod/Fem/Resources/ui)

SET_BIN_DIR(FemGui FemGui /Mod/Fem)
SET_PYTHON_PREFIX_SUFFIX(FemGui)

INSTALL(TARGETS FemGui DESTINATION ${CMAKE_INSTALL_LIBDIR})
