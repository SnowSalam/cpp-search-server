#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
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
            if (c >= '\0' && c < ' ') {
                throw invalid_argument("Words contain special symbols");
            }
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    Document() = default;

    Document(int id, double relevance, int rating)
        : id(id)
        , relevance(relevance)
        , rating(rating) {
    }

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    set<string> non_empty_strings;
    for (const string& str : strings) {
        if (!str.empty()) {
            if (any_of(str.begin(), str.end(), [](char c) { return c >= '\0' && c < ' '; })) {
                throw invalid_argument("Words contain special symbols");
            }
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    explicit SearchServer(const vector<string>& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {}

    explicit SearchServer(const set<string>& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {}

    explicit SearchServer(const string& stop_words_text)
        : SearchServer(
            SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
    {}

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {

        if (count(documents_ids_.begin(), documents_ids_.end(), document_id) == 0 && document_id >= 0) {
            documents_ids_.push_back(document_id);
        }
        else {
            if (document_id < 0) {
                throw invalid_argument("The document was not added because its id is negative");
            }
            throw invalid_argument("The document was not added because its id matches an existing one");
        }

        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    }

    int GetDocumentId(int index) {
        return documents_ids_.at(index);
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
        const Query query = ParseQuery(raw_query);

        auto matched_documents = FindAllDocuments(query, document_predicate);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < EPSILON) {
                    return lhs.rating > rhs.rating;
                }
                return lhs.relevance > rhs.relevance;
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }

        return matched_documents;
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(
            raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
                return document_status == status;
            });
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {

        const Query query = ParseQuery(raw_query);

        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }

        return { matched_words, documents_.at(document_id).status };
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    vector<int> documents_ids_;

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

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        if (text[0] == '-') {
            throw invalid_argument("Word contains an extra-minus");
        }
        if (is_minus && text.empty()) {
            throw invalid_argument("No word after minus");
        }
        return { text, is_minus, IsStopWord(text) };
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                }
                else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query,
        DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                { document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }
};

// ==================== Для примера =========================

void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s << endl;
}

int main() {
    // testing constructor

    try {
        SearchServer test_constructor1("and in wi\tth"s);
    }
    catch (const invalid_argument& test) {
        cerr << "Test 1: constructor: Invalid_argument: " << test.what() << endl;
    }

    try {
        vector<string> test_stop_words{ "and\t", "in"s, "with"s };
        SearchServer test_counstuctor2(test_stop_words);
    }
    catch (const invalid_argument& test) {
        cerr << "Test 2: constructor: Invalid_argument: " << test.what() << endl;
    }

    try {
        SearchServer search_server("and in with");
    }
    catch (const invalid_argument& unknown) {
        cerr << "Constructor: Unknown invalid argument: " << unknown.what() << endl;
    }

    SearchServer search_server("and in with"s);

    //testing docs adding
    try {
        search_server.AddDocument(1, "fluffy cat fluffy tail", DocumentStatus::ACTUAL, { 7, 2, 7 });
        search_server.AddDocument(0, "white cat and fashionable collar", DocumentStatus::ACTUAL, { 8, -3 });
        search_server.AddDocument(2, "well-groomed dog expressive eyes", DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
        search_server.AddDocument(3, "well-groomed starling Evgeniy", DocumentStatus::BANNED, { 9 });
    }
    catch (const invalid_argument& unknown) {
        cerr << "Add_document: Unknown invalid argument: " << unknown.what() << endl;
    }

    try {
        search_server.AddDocument(-1, "fluffy dog and fashionable collar", DocumentStatus::ACTUAL, { 7, 2, 7 });
    }
    catch (const invalid_argument& negative_id) {
        cerr << "Add_document: negative id: Invalid_argument: " << negative_id.what() << endl;
    }

    try {
        search_server.AddDocument(1, "white starling and fashionable bell", DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    }
    catch (const invalid_argument& existing_id) {
        cerr << "Add_document: existing id: Invalid_argument: " << existing_id.what() << endl;
    }

    try {
        search_server.AddDocument(4, "big do\tg star\tling", DocumentStatus::ACTUAL, { 1, 2, 3 });
    }
    catch (const invalid_argument& special_symbols) {
        cerr << "Add_document: special symbols: Invalid_argument: " << special_symbols.what() << endl;
    }

    //testing queries
    try {
        const auto documents = search_server.FindTopDocuments("--fluffy"s);
        for (const Document& document : documents) {
            PrintDocument(document);
        }
    }
    catch (const invalid_argument& extra_minus) {
        cerr << "Find_Top_Documents: extra minus: Invalid_argument: " << extra_minus.what() << endl;
    }

    try {
        const auto documents = search_server.FindTopDocuments("fluffy well-gro\tomed cat");
        for (const Document& document : documents) {
            PrintDocument(document);
        }
    }
    catch (const invalid_argument& special_symbols) {
        cerr << "Find_Top_Documents: special symbol: Invalid_argument: " << special_symbols.what() << endl;
    }

    try {
        const auto documents = search_server.FindTopDocuments("fluffy well-groomed cat - "s);
        for (const Document& document : documents) {
            PrintDocument(document);
        }
    }
    catch (const invalid_argument& outstanding_minus) {
        cerr << "Find_Top_Documents: minus without word: Invalid_argument: " << outstanding_minus.what() << endl;
    }

    try {
        for (const Document& document : search_server.FindTopDocuments("fluffy well-groomed cat"s)) {
            PrintDocument(document);
        }
    }
    catch (const invalid_argument& unknown) {
        cerr << "Find_Top_Documents: unknown invalid argument: " << unknown.what() << endl;
    }

    try {
        cout << endl << "BANNED:"s << endl;
        for (const Document& document : search_server.FindTopDocuments("fluffy well-groomed cat"s, DocumentStatus::BANNED)) {
            PrintDocument(document);
        }
    }
    catch (const invalid_argument& unknown) {
        cerr << "Find_Top_Documents: unknown invalid argument: " << unknown.what() << endl;
    }

    return 0;
}