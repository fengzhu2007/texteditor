cmake_minimum_required(VERSION 3.14)

project(texteditor)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Core)



if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DQ_DEBUG)
endif()


message("dirss :${AUTOGEN_BUILD_DIR}")
set(RESOURCES

)

qt5_add_resources(RESOURCE_ADDED ${RESOURCES})


add_library(texteditor SHARED
    ${RESOURCE_ADDED}
    snippets/snippetparser.h snippets/snippetparser.cpp
    snippets/snippetoverlay.h snippets/snippetoverlay.cpp
    snippets/snippet.h snippets/snippet.cpp
    snippets/snippetassistcollector.h snippets/snippetassistcollector.cpp
    snippets/reuse.h
    snippets/snippeteditor.cpp snippets/snippeteditor.h
    snippets/snippetprovider.cpp snippets/snippetprovider.h
    snippets/snippetscollection.cpp snippets/snippetscollection.h
    snippets/snippetssettings.cpp snippets/snippetssettings.h

    codeassist/assistenums.h
    codeassist/assistinterface.cpp codeassist/assistinterface.h
    codeassist/assistproposalitem.cpp codeassist/assistproposalitem.h
    codeassist/assistproposaliteminterface.h
    codeassist/codeassistant.cpp codeassist/codeassistant.h
    codeassist/completionassistprovider.cpp codeassist/completionassistprovider.h
    codeassist/documentcontentcompletion.cpp codeassist/documentcontentcompletion.h
    codeassist/functionhintproposal.cpp codeassist/functionhintproposal.h
    codeassist/functionhintproposalwidget.cpp codeassist/functionhintproposalwidget.h
    codeassist/genericproposal.cpp codeassist/genericproposal.h
    codeassist/genericproposalmodel.cpp codeassist/genericproposalmodel.h
    codeassist/genericproposalwidget.cpp codeassist/genericproposalwidget.h
    codeassist/iassistprocessor.cpp codeassist/iassistprocessor.h
    codeassist/iassistproposal.cpp codeassist/iassistproposal.h
    codeassist/iassistproposalmodel.cpp codeassist/iassistproposalmodel.h
    codeassist/iassistproposalwidget.cpp codeassist/iassistproposalwidget.h
    codeassist/iassistprovider.cpp codeassist/iassistprovider.h
    codeassist/ifunctionhintproposalmodel.cpp codeassist/ifunctionhintproposalmodel.h
    codeassist/runner.cpp codeassist/runner.h
    codeassist/textdocumentmanipulator.cpp codeassist/textdocumentmanipulator.h
    codeassist/textdocumentmanipulatorinterface.h




    aggregation/aggregate.h aggregation/aggregate.cpp

    core/core_global.h
    core/icontext.h core/icontext.cpp
    core/idocument.h core/idocument.cpp
    core/textdocument.h core/textdocument.cpp
    core/icore.h core/icore.cpp
    core/find/highlightscrollbarcontroller.h core/find/highlightscrollbarcontroller.cpp
    core/find/basetextfind.h core/find/basetextfind.cpp
    core/find/ifindsupport.h core/find/ifindsupport.cpp

    utils/elidinglabel.h utils/elidinglabel.cpp
    utils/id.h utils/id.cpp
    utils/link.h utils/link.cpp
    utils/filepathinfo.h utils/filepath.h utils/filepath.cpp
    utils/fileutils.h utils/fileutils.cpp
    utils/camelcasecursor.h utils/camelcasecursor.cpp
    utils/multitextcursor.h utils/multitextcursor.cpp
    utils/textutils.h utils/textutils.cpp
    utils/uncommentselection.h utils/uncommentselection.cpp
    utils/mimeutils.h
    utils/utils_global.h
    utils/qtcassert.h utils/qtcassert.cpp
    utils/tooltip/tooltip.h utils/tooltip/tooltip.cpp
    utils/tooltip/tips.h utils/tooltip/tips.cpp
    utils/changeset.h utils/changeset.cpp
    utils/textfileformat.h utils/textfileformat.cpp
    utils/theme/theme_p.h utils/theme/theme.h utils/theme/theme.cpp
    utils/qtcsettings.h utils/qtcsettings.cpp
    utils/algorithm.h
    utils/dropsupport.h utils/dropsupport.cpp
    utils/fadingindicator.h utils/fadingindicator.cpp
    utils/fixedsizeclicklabel.h utils/fixedsizeclicklabel.cpp
    utils/minimizableinfobars.h utils/minimizableinfobars.cpp
    utils/stylehelper.h utils/stylehelper.cpp
    utils/hostosinfo.h utils/hostosinfo.cpp
    utils/infobar.h utils/infobar.cpp
    utils/styledbar.h utils/styledbar.cpp
    utils/faketooltip.h utils/faketooltip.cpp
    utils/runextensions.h utils/runextensions.cpp



    utils/mimetypes2/mimedatabase.cpp
    utils/mimetypes2/mimedatabase.h
    utils/mimetypes2/mimedatabase_p.h
    utils/mimetypes2/mimeglobpattern.cpp
    utils/mimetypes2/mimeglobpattern_p.h
    utils/mimetypes2/mimemagicrule.cpp
    utils/mimetypes2/mimemagicrule_p.h
    utils/mimetypes2/mimemagicrulematcher.cpp
    utils/mimetypes2/mimemagicrulematcher_p.h
    utils/mimetypes2/mimeprovider.cpp
    utils/mimetypes2/mimeprovider_p.h
    utils/mimetypes2/mimetype.cpp
    utils/mimetypes2/mimetype.h
    utils/mimetypes2/mimetype_p.h
    utils/mimetypes2/mimetypeparser.cpp
    utils/mimetypes2/mimetypeparser_p.h
    utils/mimetypes2/mimeutils.cpp
    utils/sizedarray.h
    utils/fuzzymatcher.h utils/fuzzymatcher.cpp
    utils/savefile.h utils/savefile.cpp
    utils/filesearch.h utils/filesearch.cpp
    utils/stringutils.h utils/stringutils.cpp
    utils/mapreduce.h




    syntax-highlighting/ksyntaxhighlighting_logging.cpp syntax-highlighting/ksyntaxhighlighting_logging.h
    syntax-highlighting/ksyntaxhighlighting_version.h
    syntax-highlighting/ksyntaxhighlighting_export.h
    syntax-highlighting/abstracthighlighter.cpp syntax-highlighting/abstracthighlighter.h syntax-highlighting/abstracthighlighter_p.h
    syntax-highlighting/context.cpp syntax-highlighting/context_p.h
    syntax-highlighting/contextswitch.cpp syntax-highlighting/contextswitch_p.h
    syntax-highlighting/definition.cpp syntax-highlighting/definition.h
    syntax-highlighting/definitiondownloader.cpp syntax-highlighting/definitiondownloader.h
    syntax-highlighting/definitionref_p.h
    syntax-highlighting/definition_p.h
    syntax-highlighting/foldingregion.cpp syntax-highlighting/foldingregion.h
    syntax-highlighting/format.cpp syntax-highlighting/format.h syntax-highlighting/format_p.h
    syntax-highlighting/highlightingdata.cpp syntax-highlighting/highlightingdata_p.hpp
    syntax-highlighting/keywordlist.cpp syntax-highlighting/keywordlist_p.h
    syntax-highlighting/matchresult_p.h
    syntax-highlighting/repository.cpp syntax-highlighting/repository.h syntax-highlighting/repository_p.h
    syntax-highlighting/rule.cpp syntax-highlighting/rule_p.h
    syntax-highlighting/state.cpp syntax-highlighting/state.h syntax-highlighting/state_p.h
    syntax-highlighting/syntaxhighlighter.cpp syntax-highlighting/syntaxhighlighter.h
    syntax-highlighting/textstyledata_p.h
    syntax-highlighting/theme.cpp syntax-highlighting/theme.h
    syntax-highlighting/themedata.cpp syntax-highlighting/themedata_p.h
    syntax-highlighting/wildcardmatcher.cpp syntax-highlighting/wildcardmatcher_p.h
    syntax-highlighting/worddelimiters.cpp syntax-highlighting/worddelimiters_p.h
    syntax-highlighting/xml_p.h

    indenter.h
    linecolumn.h linecolumn.cpp
    textstyles.h
    texteditor.h texteditor.cpp
    osspecificaspects.h
    refactoroverlay.h refactoroverlay.cpp
    predicates.h
    texteditorconstants.h
    displaysettings.h displaysettings.cpp
    marginsettings.h marginsettings.cpp
    fontsettings.h fontsettings.cpp
    autocompleter.h autocompleter.cpp
    basehoverhandler.h basehoverhandler.cpp
    behaviorsettings.h behaviorsettings.cpp
    circularclipboard.h circularclipboard.cpp
    circularclipboardassist.h circularclipboardassist.cpp
    completionsettings.h completionsettings.cpp
    extraencodingsettings.h extraencodingsettings.cpp
    highlighter.h highlighter.cpp
    highlightersettings.h highlightersettings.cpp
    icodestylepreferences.h icodestylepreferences.cpp
    storagesettings.h storagesettings.cpp
    syntaxhighlighter.h syntaxhighlighter.cpp
    tabsettings.h tabsettings.cpp
    textdocument.h textdocument.cpp
    textdocumentlayout.h textdocumentlayout.cpp
    texteditoroverlay.h texteditoroverlay.cpp
    texteditorsettings.h texteditorsettings.cpp
    typingsettings.h typingsettings.cpp
    quickfix.h quickfix.cpp
    commentssettings.h commentssettings.cpp
    refactoringchanges.h refactoringchanges.cpp
    colorscheme.h colorscheme.cpp
    textmark.h textmark.cpp
    behaviorsettingswidget.h behaviorsettingswidget.cpp
    simplecodestylepreferenceswidget.h simplecodestylepreferenceswidget.cpp
    codestylepool.h codestylepool.cpp
    textindenter.h textindenter.cpp
    icodestylepreferencesfactory.h icodestylepreferencesfactory.cpp
    texteditorconstants.h texteditorconstants.cpp
    codecchooser.h codecchooser.cpp
    tabsettingswidget.h tabsettingswidget.cpp
    codestyleeditor.h codestyleeditor.cpp
    codestyleselectorwidget.h codestyleselectorwidget.cpp
    javaindenter.h javaindenter.cpp
    resource.qrc
    languages/python/pythonhighlighter.cpp languages/python/pythonhighlighter.h languages/python/pythonindenter.cpp languages/python/pythonindenter.h languages/python/pythonscanner.cpp languages/python/pythonscanner.h languages/python/pythonformattoken.h
    languages/cstyle/cstyleindenter.h languages/cstyle/cstyleindenter.cpp
    languages/cstyle/cstyleqtcodeformatter.h languages/cstyle/cstyleqtcodeformatter.cpp
    languages/cstyle/cstylecodeformatter.h languages/cstyle/cstylecodeformatter.cpp
    languages/cstyle/cstylescanner.h languages/cstyle/cstylescanner.cpp
    languages/cstyle/cstyleautocompleter.h languages/cstyle/cstyleautocompleter.cpp
    languages/cstyle/cstylehighlighter.h languages/cstyle/cstylehighlighter.cpp
    codeformatter.h codeformatter.cpp
    languages/html/htmlscanner.h languages/html/htmlscanner.cpp
    languages/token.h
    languages/php/phpscanner.h languages/php/phpscanner.cpp
    languages/php/phpautocompleter.h languages/php/phpautocompleter.cpp languages/php/phpindenter.h languages/php/phpindenter.cpp languages/php/phpcodeformatter.h languages/php/phpcodeformatter.cpp
    languages/javascript/jsscanner.h languages/javascript/jsscanner.cpp
    languages/css/cssscanner.h languages/css/cssscanner.cpp
    languages/html/htmlcodeformatter.h languages/html/htmlcodeformatter.cpp
    languages/html/htmlindenter.h
    languages/html/htmlindenter.cpp
    languages/html/htmlautocompleter.h languages/html/htmlautocompleter.cpp
    languages/html/htmlhighlighter.h languages/html/htmlhighlighter.cpp
    languages/javascript/jscodeformatter.h languages/javascript/jscodeformatter.cpp
    languages/css/csscodeformatter.h languages/css/csscodeformatter.cpp
    languages/loader.h languages/loader.cpp
    languages/javascript/jsautocompleter.cpp languages/javascript/jsautocompleter.h languages/javascript/jshighlighter.cpp languages/javascript/jshighlighter.h languages/javascript/jsindenter.cpp languages/javascript/jsindenter.h
    languages/css/cssautocompleter.cpp languages/css/cssautocompleter.h languages/css/csshighlighter.cpp languages/css/csshighlighter.h languages/css/cssindenter.cpp languages/css/cssindenter.h
    languages/jsx/jsxscanner.h languages/jsx/jsxscanner.cpp











)

target_link_libraries(texteditor PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Core)

target_compile_definitions(texteditor PRIVATE TEXTEDITOR_LIBRARY)


add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy  ${CMAKE_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}.dll        ${CMAKE_BINARY_DIR}/src/${PROJECT_NAME}.dll
    COMMENT "Copying file to binary directory"
)

