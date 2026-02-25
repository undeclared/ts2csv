#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <map>
#include <regex>

using namespace std;

void processTsFileLine(string& line, vector<string>& translationStrings)
{
    if (line.find("<source>") == string::npos) {
        return;
    }

    size_t start = line.find("<source>") + 8;
    size_t end = line.find("</source>");
    string str = line.substr(start, end - start);

    translationStrings.push_back(str);
}

void processCsvFileLine(string& line, vector<string>& row, vector<vector<string>>& csvContent)
{
    row.clear();
    stringstream ss(line);
    string field;
    
    bool inQuotes = false;
    char prevChar = '\0';
    
    for (size_t i = 0; i < line.length(); i++) {
        char c = line[i];
        
        if (c == '"' && prevChar != '\\') {
            inQuotes = !inQuotes;
        } else if (c == ',' && !inQuotes) {
            row.push_back(field);
            field.clear();
        } else {
            field += c;
        }
        
        prevChar = c;
    }
    
    if (!field.empty()) {
        row.push_back(field);
    }

    csvContent.push_back(row);
}

void showInstructions()
{
    cout << "ts2csv project.ts"
         << "\n\nCSV to TS mode:"
         << "\nts2csv project.ts output.csv"
         << endl;
}

map<string, map<string, string>> generateReplacementsByLanguageMap(vector<vector<string>>& csvContent, vector<string>& languages)
{
    map<string, map<string, string>> replacementsByLanguage;

    for (string& language : languages) {
        replacementsByLanguage[language] = map<string, string>();
    }

    bool isHeader = true;

    for (vector<string>& content : csvContent) {
        if (isHeader) {
            isHeader = false;
            continue;
        }

        if (content.empty()) continue;
        
        string sourceString = content[0];

        for (int i = 1; i <= languages.size(); i++) {
            string lang = languages[i - 1];

            if (lang.empty()) {
                continue;
            }

            map<string, string>& langMap = replacementsByLanguage[lang];

            if (i < content.size()) {
                langMap[sourceString] = content[i];
            } else {
                langMap[sourceString] = string();
            }
        }
    }

    return replacementsByLanguage;
}

vector<string> getLanguagesFromHeaders(vector<string>& headers)
{
    vector<string> languages;

    for (size_t i = 1; i < headers.size(); i++) {
        languages.push_back(headers[i]);
    }

    return languages;
}

string unescapeString(const string& input) {
    string output;
    output.reserve(input.size());
    
    for (size_t i = 0; i < input.size(); i++) {
        if (input[i] == '\\' && i + 1 < input.size()) {
            switch (input[i + 1]) {
                case 'n': output += '\n'; i++; break;
                case 't': output += '\t'; i++; break;
                case 'r': output += '\r'; i++; break;
                case '\\': output += '\\'; i++; break;
                case '"': output += '"'; i++; break;
                default: output += input[i]; break;
            }
        } else {
            output += input[i];
        }
    }
    
    return output;
}

string escapeXml(const string& input) {
    string output;
    output.reserve(input.size());
    
    for (char c : input) {
        switch (c) {
            case '&':  output += "&amp;";  break;
            case '<':  output += "&lt;";   break;
            case '>':  output += "&gt;";   break;
            case '"':  output += "&quot;"; break;
            case '\'': output += "&apos;"; break;
            default:   output += c;       break;
        }
    }
    
    return output;
}

string escapeForCsv(const string& input) {
    string output;
    output.reserve(input.size());
    
    for (char c : input) {
        switch (c) {
            case '\n': output += "\\n"; break;
            case '\t': output += "\\t"; break;
            case '\r': output += "\\r"; break;
            case '\\': output += "\\\\"; break;
            case '"': output += "\\\""; break;
            default: output += c; break;
        }
    }
    
    return output;
}

void generateTsFiles(vector<vector<string>>& csvContent, string& filename, string& csvFilename)
{
    vector<string> headers = csvContent[0];
    
    // Clean headers (remove quotes if present)
    for (auto& header : headers) {
        if (!header.empty() && header.front() == '"' && header.back() == '"') {
            header = header.substr(1, header.size() - 2);
        }
    }
    
    vector<string> languages = getLanguagesFromHeaders(headers);

    ifstream t(filename);
    stringstream buffer;
    buffer << t.rdbuf();
    string tsFileContents = buffer.str();
    t.close();

    const string TRANSLATION_OPEN_TAG = "<translation";
    const string TRANSLATION_CLOSE_TAG = "</translation>";

    // Create a map for quick lookup of translations
    map<string, map<string, string>> translationsByLang;
    
    // First, populate the map from CSV data
    bool isHeader = true;
    for (vector<string>& row : csvContent) {
        if (isHeader) {
            isHeader = false;
            continue;
        }
        
        if (row.empty()) continue;
        
        string source = row[0];
        if (!source.empty() && source.front() == '"' && source.back() == '"') {
            source = source.substr(1, source.size() - 2);
        }
        
        for (size_t i = 1; i <= languages.size(); i++) {
            if (i - 1 >= languages.size()) continue;
            
            string lang = languages[i - 1];
            if (lang.empty()) continue;
            
            string translation;
            if (i < row.size()) {
                translation = row[i];
                if (!translation.empty() && translation.front() == '"' && translation.back() == '"') {
                    translation = translation.substr(1, translation.size() - 2);
                }
                translation = unescapeString(translation);
            }
            
            translationsByLang[lang][source] = translation;
        }
    }

    // Generate TS file for each language
    for (string &language: languages) {
        if (language.empty()) continue;
        
        string langTsFileContents = tsFileContents;
        size_t pos = 0;
        
        // Find and replace each translation
        while (pos < langTsFileContents.length()) {
            // Find source tag
            size_t sourceStart = langTsFileContents.find("<source>", pos);
            if (sourceStart == string::npos) break;
            
            size_t sourceEnd = langTsFileContents.find("</source>", sourceStart);
            if (sourceEnd == string::npos) break;
            
            string source = langTsFileContents.substr(sourceStart + 8, sourceEnd - sourceStart - 8);
            
            // Find translation tag
            size_t transStart = langTsFileContents.find(TRANSLATION_OPEN_TAG, sourceEnd);
            if (transStart == string::npos) {
                pos = sourceEnd + 9;
                continue;
            }
            
            size_t transEnd = langTsFileContents.find(TRANSLATION_CLOSE_TAG, transStart);
            if (transEnd == string::npos) {
                pos = sourceEnd + 9;
                continue;
            }
            
            transEnd += TRANSLATION_CLOSE_TAG.length();
            
            // Get translation from map
            string translation;
            auto langIt = translationsByLang.find(language);
            if (langIt != translationsByLang.end()) {
                auto transIt = langIt->second.find(source);
                if (transIt != langIt->second.end()) {
                    translation = escapeXml(transIt->second);
                }
            }
            
            // Build new translation string
            string translationString = "<translation>";
            if (!translation.empty()) {
                translationString += translation;
            }
            translationString += "</translation>";
            
            // Replace in content
            langTsFileContents.replace(transStart, transEnd - transStart, translationString);
            
            // Move position after the replaced translation
            pos = transStart + translationString.length();
        }

        // Write to file
        string outputFilename = language + ".ts";
        ofstream file(outputFilename, ios::out | ios::binary);
        if (!file.is_open()) {
            cout << "File could not be created: " << outputFilename << endl;
            continue;
        }
        
        file << langTsFileContents;
        file.close();
        
        cout << "Created: " << outputFilename << endl;
    }
}

void generateCsv(vector<string>& translationStrings)
{
    // Remove duplicates
    map<string, bool> seen;
    vector<string> uniqueStrings;
    
    for (auto& str : translationStrings) {
        if (!seen[str]) {
            uniqueStrings.push_back(str);
            seen[str] = true;
        }
    }

    ofstream csvFile("output.csv", ios::out | ios::trunc);

    // Write header
    csvFile << "\"Source\",\"English\",\"Spanish\",\"French\",\"German\",\"Russian\"\n";

    if (!csvFile.is_open()) {
        cout << "Could not create file: output.csv" << endl;
        exit(1);
    }

    // Write data rows
    for (auto& translationString : uniqueStrings) {
        // Escape the source string for CSV
        string escapedSource = translationString;
        // Replace quotes with escaped quotes
        size_t quotePos = escapedSource.find('"');
        while (quotePos != string::npos) {
            escapedSource.replace(quotePos, 1, "\"\"");
            quotePos = escapedSource.find('"', quotePos + 2);
        }
        
        csvFile << "\"" << escapedSource << "\"";
        
        // Add empty columns for translations
        for (int i = 0; i < 5; i++) {
            csvFile << ",\"\"";
        }
        csvFile << endl;
    }

    csvFile.close();
    cout << "Created: output.csv" << endl;
}

int main(int argc, char *argv[])
{
    if (argc == 1) {
        showInstructions();
        return 0;
    }

    string filename = string(argv[1]);
    bool isCsv = argc == 3;
    string csvFilename;

    if (argc == 3) {
        csvFilename = argv[2];
    }

    string line;

    string openFile = csvFilename.empty() ? filename : csvFilename;
    ifstream file(openFile);

    vector<vector<string>> csvContent;
    vector<string> row;
    vector<string> translationStrings;

    if (!file.is_open()) {
        cout << "File could not be opened: " << openFile << endl;
        return 1;
    }

    if (isCsv) {
        // Read CSV file
        while (getline(file, line)) {
            processCsvFileLine(line, row, csvContent);
        }
        
        if (csvContent.empty()) {
            cout << "No content found in CSV!" << endl;
            return 0;
        }
        
        generateTsFiles(csvContent, filename, csvFilename);

    } else {
        // Read TS file
        while (getline(file, line)) {
            processTsFileLine(line, translationStrings);
        }
        
        if (translationStrings.empty()) {
            cout << "No translation strings found in TS file!" << endl;
            return 0;
        }
        
        generateCsv(translationStrings);
    }

    file.close();
    return 0;
}
