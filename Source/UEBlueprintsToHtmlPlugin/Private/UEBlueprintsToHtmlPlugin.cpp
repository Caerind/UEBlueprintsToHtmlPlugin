#include "UEBlueprintsToHtmlPlugin.h"

#include "Editor.h"
#include "ToolMenus.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Editor/UnrealEd/Public/EdGraphUtilities.h"

#define LOCTEXT_NAMESPACE "FUEBlueprintsToHtmlPluginModule"

void FUEBlueprintsToHtmlPluginModule::StartupModule()
{
    RegisterMenus();
}

void FUEBlueprintsToHtmlPluginModule::ShutdownModule()
{
    if (UToolMenus::IsToolMenuUIEnabled())
    {
        UToolMenus::UnregisterOwner(this);
    }
}

void FUEBlueprintsToHtmlPluginModule::RegisterMenus()
{
    if (!UToolMenus::IsToolMenuUIEnabled())
    {
        return;
    }

    UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
    FToolMenuSection& Section = ToolsMenu->AddSection("UEBlueprintsToHtmlPlugin", FText::FromString("UEBlueprintsToHtmlPlugin"));
    Section.AddMenuEntry(
        "DumpBlueprintsToHtml",
        FText::FromString("DumpBlueprintsToHtml"),
        FText::FromString("DumpBlueprintsToHtml"),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateRaw(this, &FUEBlueprintsToHtmlPluginModule::DumpBlueprintsToHtml))
    );
}

FString SanitizeFileName(const FString& inputName)
{
    FString sanitizedFileName;
    sanitizedFileName = FPaths::MakeValidFileName(inputName);
    sanitizedFileName.ReplaceInline(TEXT("/"), TEXT(""));
    sanitizedFileName.ReplaceInline(TEXT("\\"), TEXT(""));
    sanitizedFileName.ReplaceInline(TEXT(" / "), TEXT("_"));
    sanitizedFileName.ReplaceInline(TEXT(" "), TEXT("_"));
    return sanitizedFileName;
}

void FUEBlueprintsToHtmlPluginModule::DumpBlueprintsToHtml()
{
    FString FolderPath = FPaths::ProjectSavedDir() / TEXT("DumpBlueprintsToHtml");
    IFileManager& FileManager = IFileManager::Get();

    FileManager.DeleteDirectory(*FolderPath, /*RequireExists=*/false, /*Tree=*/true); 
    FileManager.MakeDirectory(*FolderPath);

    TArray<UObject*> Blueprints;
    GetObjectsOfClass(UBlueprint::StaticClass(), Blueprints, true, RF_ClassDefaultObject); 

    UE_LOG(LogTemp, Log, TEXT("Total number of Blueprints found: %d"), Blueprints.Num());

    FString GlobalHTMLContent;
    GlobalHTMLContent += TEXT("<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"UTF-8\">\n<title>Blueprint Dump</title>\n</head>\n<body>\n");
    GlobalHTMLContent += TEXT("<h1>Blueprints and Graphs</h1>\n<ul>\n");

    for (UObject* Object : Blueprints)
    {
        UBlueprint* Blueprint = Cast<UBlueprint>(Object);
        if (Blueprint /* && Blueprint->GeneratedClass*/)
        {
            FString PackagePath = Blueprint->GetOutermost()->GetName();
            FString ContentPath = FPaths::ProjectContentDir();
            FString FullPath;
            bool IsUnderContentFolder = FPackageName::DoesPackageExist(PackagePath, &FullPath) && FullPath.StartsWith(ContentPath);
            if (!IsUnderContentFolder)
            {
                continue;
            }

            FString BlueprintName = Blueprint->GetName();
            FString SanitizedBlueprintName = SanitizeFileName(BlueprintName);

            TArray<UEdGraph*> AllGraphs;
            Blueprint->GetAllGraphs(AllGraphs);

            bool WriteBlueprint = false;
            for (const UEdGraph* Graph : AllGraphs)
            {
                if (Graph != nullptr && Graph->Nodes.Num() > 0)
                {
                    WriteBlueprint = true;
                    break;
                }
            }
            if (!WriteBlueprint)
                continue;

            UE_LOG(LogTemp, Log, TEXT("Blueprint: %s"), *BlueprintName);

            GlobalHTMLContent += FString::Printf(TEXT("<li><strong>%s</strong>\n<ul>\n"), *BlueprintName);

            int32 TransitionCount = 0;
            for (const UEdGraph* Graph : AllGraphs)
            {
                if (Graph != nullptr && Graph->Nodes.Num() > 0)
                {
                    FString GraphName = Graph->GetName();
                    UE_LOG(LogTemp, Log, TEXT("  Graph: %s"), *GraphName);

                    FString NodesString;
                    TSet<UObject*> Nodes;
                    Nodes.Reserve(Graph->Nodes.Num());
                    for (UEdGraphNode* Node : Graph->Nodes)
                    {
                        Nodes.Add(Node);
                    }
                    FEdGraphUtilities::ExportNodesToText(Nodes, NodesString);

                    // Special handling for Transition in ABP (not sure how to improve that yet...)
                    if (Graph->GetSchema() != nullptr &&
                        Graph->GetSchema()->GetGraphType(Graph) == GT_Function &&
                        GraphName.Equals(TEXT("Transition")) &&
                        BlueprintName.StartsWith(TEXT("ABP")))
                    {
                        GraphName += FString::Printf(TEXT("_%d"), TransitionCount++);
                    }

                    FString SanitizedGraphName = SanitizeFileName(GraphName);

                    FString HTMLContent = FString::Printf(
                        TEXT("<!DOCTYPE html>\n"
                            "<html lang=\"en\">\n"
                            "<head>\n"
                            "    <meta charset=\"UTF-8\">\n"
                            "    <title>%s</title>\n"
                            "    <link href=\"https://blueprintue.com/bue-render/render.css\" rel=\"stylesheet\">\n"
                            "    <style>.hidden{display: none;}</style>\n"
                            "</head>\n"
                            "<body>\n" 
                            "    <a href=\"index.html\">Back to Index</a>\n"
                            "    <div class=\"playground\"></div>\n"
                            "    <textarea class=\"hidden\" id=\"pastebin_data\">%s</textarea>\n"
                            "    <script src=\"https://blueprintue.com/bue-render/render.js\"></script>\n"
                            "    <script>\n"
                            "        new window.blueprintUE.render.Main(\n"
                            "            document.getElementById('pastebin_data').value,\n"
                            "            document.getElementsByClassName('playground')[0],\n"
                            "            {height:\"643px\"}\n"
                            "        ).start();\n"
                            "    </script>\n"
                            "</body>\n"
                            "</html>"),
                        *GraphName,
                        *NodesString
                    );
                    FString HTMLFilePath = FolderPath / FString::Printf(TEXT("%s_%s.html"), *SanitizedBlueprintName, *SanitizedGraphName);
                    FFileHelper::SaveStringToFile(HTMLContent, *HTMLFilePath);

                    FString RelativeHTMLPath = FString::Printf(TEXT("%s_%s.html"), *SanitizedBlueprintName, *SanitizedGraphName);
                    GlobalHTMLContent += FString::Printf(TEXT("<li><a href=\"%s\">%s</a></li>\n"), *RelativeHTMLPath, *GraphName);
                }
            }

            GlobalHTMLContent += TEXT("</ul></li>\n");
        }
    }

    GlobalHTMLContent += TEXT("</ul>\n</body>\n</html>");

    FString GlobalHTMLFilePath = FolderPath / TEXT("index.html");
    FFileHelper::SaveStringToFile(GlobalHTMLContent, *GlobalHTMLFilePath);

    UE_LOG(LogTemp, Log, TEXT("Dump completed. Check the output in: %s"), *FolderPath);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUEBlueprintsToHtmlPluginModule, UEBlueprintsToHtmlPlugin)
