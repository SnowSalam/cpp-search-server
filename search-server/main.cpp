#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
int DOCUMENT_COUNT = 0;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result = 0;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;

    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }

    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
};

struct Query {
    set<string> minus;
    set<string> plus;
};

class SearchServer {
public:

    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(const int& document_id, const string& document) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        double TF_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += TF_count;
        }
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        const Query query_words = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query_words);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                return lhs.relevance > rhs.relevance;
            });

        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }

        return matched_documents;
    }

private:

    map<string, map<int, double>> word_to_document_freqs_; //<word, <doc_id, TF>>

    set<string> stop_words_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;

        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }

        return words;
    }

    Query ParseQuery(const string& text) const {
        Query query_words;

        for (const string& word : SplitIntoWordsNoStop(text)) {
            if (word[0] == '-') {
                query_words.minus.insert(word.substr(1));
            }
            else {
                query_words.plus.insert(word);
            }
        }

        return query_words;
    }

    vector<Document> FindAllDocuments(const Query& query_words) const {
        map<int, double> matched_documents; //<doc_id, counted relevance>

        for (const string& plus_word : query_words.plus) {
            if (word_to_document_freqs_.count(plus_word) > 0) {
                double idf_count = log((double)DOCUMENT_COUNT / word_to_document_freqs_.at(plus_word).size());
                for (const auto& [id, tf] : word_to_document_freqs_.at(plus_word)) {
                    matched_documents[id] += tf * idf_count;
                }
            }
        }

        for (const string& minus_word : query_words.minus) {
            if (word_to_document_freqs_.count(minus_word) > 0) {
                for (const pair<int, double>& id_tf : word_to_document_freqs_.at(minus_word)) {
                    if (matched_documents.count(id_tf.first) > 0) {
                        matched_documents.erase(id_tf.first);
                    }
                }
            }
        }

        vector<Document> matched_docs_vector;
        for (const auto& [id, relevance] : matched_documents) {
            vector_matched.push_back({ id, relevance });
        }

        return matched_docs_vector;
    }

};

SearchServer CreateSearchServer() {
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());

    DOCUMENT_COUNT = ReadLineWithNumber();
    for (int document_id = 0; document_id < DOCUMENT_COUNT; ++document_id) {
        search_server.AddDocument(document_id, ReadLine());
    }

    return search_server;
}

int main() {
    const SearchServer search_server = CreateSearchServer();

    const string query = ReadLine();
    for (const auto& [document_id, relevance] : search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "s << document_id << ", "
            << "relevance = "s << relevance << " }"s << endl;
    }

    return 0;
}