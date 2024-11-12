set(
	UCHARDET_HEADERS
	uchardet.h
)

set(
	UCHARDET_SOURCES
	CharDistribution.cpp
	JpCntx.cpp
	LangModels/LangArabicModel.cpp
	LangModels/LangBelarusianModel.cpp
	LangModels/LangBulgarianModel.cpp
	LangModels/LangCatalanModel.cpp
    LangModels/LangCroatianModel.cpp
    LangModels/LangCzechModel.cpp
    LangModels/LangEnglishModel.cpp
	LangModels/LangEsperantoModel.cpp
	LangModels/LangEstonianModel.cpp
	LangModels/LangFinnishModel.cpp
	LangModels/LangFrenchModel.cpp
	LangModels/LangDanishModel.cpp
	LangModels/LangGermanModel.cpp
	LangModels/LangGeorgianModel.cpp
	LangModels/LangGreekModel.cpp
	LangModels/LangHungarianModel.cpp
	LangModels/LangHebrewModel.cpp
	LangModels/LangHindiModel.cpp
	LangModels/LangIrishModel.cpp
	LangModels/LangItalianModel.cpp
    LangModels/LangLithuanianModel.cpp
    LangModels/LangLatvianModel.cpp
    LangModels/LangMacedonianModel.cpp
    LangModels/LangMalteseModel.cpp
    LangModels/LangPolishModel.cpp
    LangModels/LangPortugueseModel.cpp
	LangModels/LangRomanianModel.cpp
	LangModels/LangRussianModel.cpp
	LangModels/LangSerbianModel.cpp
	LangModels/LangSlovakModel.cpp
    LangModels/LangSloveneModel.cpp
    LangModels/LangSwedishModel.cpp
	LangModels/LangSpanishModel.cpp
	LangModels/LangThaiModel.cpp
	LangModels/LangTurkishModel.cpp
	LangModels/LangUkrainianModel.cpp
	LangModels/LangVietnameseModel.cpp
	LangModels/LangNorwegianModel.cpp
	nsHebrewProber.cpp
	nsCharSetProber.cpp
	nsBig5Prober.cpp
	nsEUCJPProber.cpp
	nsEUCKRProber.cpp
	nsEUCTWProber.cpp
	nsEscCharsetProber.cpp
	nsEscSM.cpp
	nsGB2312Prober.cpp
	nsJohabProber.cpp
	nsMBCSGroupProber.cpp
	nsMBCSSM.cpp
	nsSBCSGroupProber.cpp
	nsSBCharSetProber.cpp
	nsSJISProber.cpp
	nsUTF8Prober.cpp
	nsLanguageDetector.cpp
	nsCJKDetector.cpp
	nsLatin1Prober.cpp
	nsUniversalDetector.cpp
	uchardet.cpp
)



set (UCHARDET_STATIC_LIBRARY uchardet)

add_definitions(
	-DVERSION="${UCHARDET_VERSION}"
	-Wall
)



add_library(${UCHARDET_STATIC_LIBRARY} STATIC ${UCHARDET_SOURCES} )
	
target_compile_definitions("${UCHARDET_STATIC_LIBRARY}" PRIVATE BUILDING_UCHARDET)

target_include_directories(${UCHARDET_STATIC_LIBRARY}
		PUBLIC
			"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
			"$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${PACKAGE_NAME}>"
	)

set_target_properties(
		${UCHARDET_STATIC_LIBRARY}
		PROPERTIES
			LINKER_LANGUAGE
				CXX
			OUTPUT_NAME
				${PACKAGE_NAME}
	)

