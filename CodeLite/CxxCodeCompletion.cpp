#include "CxxCodeCompletion.hpp"

#include "CxxExpression.hpp"
#include "CxxScannerTokens.h"
#include "CxxVariableScanner.h"
#include "ctags_manager.h"
#include "file_logger.h"
#include "fileextmanager.h"
#include "function.h"
#include "language.h"

#include <algorithm>
#include <deque>

namespace
{
TagEntryPtr create_global_scope_tag()
{
    TagEntryPtr global_scope(new TagEntry());
    global_scope->SetName("<global>");
    global_scope->SetPath("<global>");
    return global_scope;
}

wxArrayString to_wx_array_string(const vector<wxString>& v)
{
    wxArrayString a;
    a.reserve(v.size());
    for(const wxString& s : v) {
        a.Add(s);
    }
    return a;
}

} // namespace

#define RECURSE_GUARD_RETURN_NULLPTR() \
    m_recurse_protector++;             \
    if(m_recurse_protector > 150) {    \
        return nullptr;                \
    }

#define RECURSE_GUARD_RETURN()      \
    m_recurse_protector++;          \
    if(m_recurse_protector > 150) { \
        return;                     \
    }

CxxCodeCompletion::CxxCodeCompletion(ITagsStoragePtr lookup)
    : m_lookup(lookup)
{
    m_template_manager.reset(new TemplateManager(this));
}

CxxCodeCompletion::~CxxCodeCompletion() {}

TagEntryPtr CxxCodeCompletion::determine_current_scope()
{
    if(!m_current_scope_name.empty() || m_filename.empty() || m_line_number == wxNOT_FOUND) {
        return m_current_scope_tag;
    }

    if(!m_lookup) {
        return nullptr;
    }

    m_current_scope_tag = m_lookup->GetScope(m_filename, m_line_number);
    if(m_current_scope_tag) {
        m_current_scope_name = m_current_scope_tag->GetScope();
    }
    return m_current_scope_tag;
}

TagEntryPtr CxxCodeCompletion::code_complete(const wxString& expression, const vector<wxString>& visible_scopes,
                                             CxxRemainder* remainder)
{
    // build expression from the expression
    m_recurse_protector = 0;
    m_template_manager.reset(new TemplateManager(this));

    vector<wxString> scopes = { visible_scopes.begin(), visible_scopes.end() };
    vector<CxxExpression> expr_arr = CxxExpression::from_expression(expression, remainder);
    auto where =
        find_if(visible_scopes.begin(), visible_scopes.end(), [](const wxString& scope) { return scope.empty(); });

    if(where == visible_scopes.end()) {
        // add the global scope
        scopes.push_back(wxEmptyString);
    }

    // Check the current scope
    if(!m_current_scope_name.empty()) {
        prepend_scope(scopes, m_current_scope_name);
    }

    clDEBUG() << "code_complete() called with scopes:" << scopes << endl;

    // handle global scope
    if(expr_arr.empty()) {
        // user types Ctrl+SPACE
        return nullptr;
    } else if(expr_arr.size() == 1 && expr_arr[0].type_name().empty() && expr_arr[0].operand_string() == "::") {
        // return a dummy entry representing the global scope
        return create_global_scope_tag();
    } else if(expr_arr.size() >= 2 && expr_arr[0].type_name().empty() && expr_arr[0].operand_string() == "::") {
        // explicity requesting for the global namespace
        // clear the `scopes` and use only the global namespace (empty string)
        scopes.clear();
        scopes.push_back(wxEmptyString);
        // in addition, we can remove the first expression from the array
        expr_arr.erase(expr_arr.begin());
    }
    m_first_time = true;
    return resolve_compound_expression(expr_arr, scopes, {});
}

TagEntryPtr CxxCodeCompletion::resolve_compound_expression(vector<CxxExpression>& expression,
                                                           const vector<wxString>& visible_scopes,
                                                           const CxxExpression& orig_expression)
{
    RECURSE_GUARD_RETURN_NULLPTR();

    // resolve each expression
    if(expression.empty()) {
        return nullptr;
    }

    TagEntryPtr resolved;
    for(CxxExpression& curexpr : expression) {
        if(orig_expression.check_subscript_operator()) {
            curexpr.set_subscript_params(orig_expression.subscript_params());
        }
        resolved = resolve_expression(curexpr, resolved, visible_scopes);
        CHECK_PTR_RET_NULL(resolved);
        // once we resolved something we make it with this flag
        // this way we avoid checking for locals/global scope etc
        // since we already have some context
        m_first_time = false;
    }
    return resolved;
}

size_t CxxCodeCompletion::parse_locals(const wxString& text, unordered_map<wxString, __local>* locals) const
{
    shrink_scope(text, locals);
    return locals->size();
}

wxString CxxCodeCompletion::shrink_scope(const wxString& text, unordered_map<wxString, __local>* locals) const
{
    CxxVariableScanner scanner(text, eCxxStandard::kCxx11, get_tokens_map(), false);
    const wxString& trimmed_text = scanner.GetOptimizeBuffer();

    CxxVariable::Vec_t variables = scanner.GetVariables(false);
    locals->reserve(variables.size());

    for(auto var : variables) {
        __local local;
        wxString assignement_expr_raw = var->GetDefaultValue();
        // strip it from any keywords etc and only keep interesting parts

        local.set_assignment_raw(assignement_expr_raw);
        local.set_assignment(assignement_expr_raw);
        local.set_type_name(var->GetTypeAsString());
        local.set_is_auto(var->IsAuto());
        local.set_name(var->GetName());
        local.set_pattern(var->ToString(CxxVariable::kToString_Name | CxxVariable::kToString_DefaultValue));
        locals->insert({ var->GetName(), local });
    }
    return trimmed_text;
}

TagEntryPtr CxxCodeCompletion::lookup_operator_arrow(TagEntryPtr parent, const vector<wxString>& visible_scopes)
{
    return lookup_child_symbol(parent, "operator->", visible_scopes, { "function", "prototype" });
}

TagEntryPtr CxxCodeCompletion::lookup_subscript_operator(TagEntryPtr parent, const vector<wxString>& visible_scopes)
{
    CHECK_PTR_RET_NULL(m_lookup);
    vector<TagEntryPtr> scopes = get_scopes(parent, visible_scopes);
    for(auto scope : scopes) {
        vector<TagEntryPtr> tags;
        m_lookup->GetSubscriptOperator(scope->GetPath(), tags);
        if(!tags.empty()) {
            return tags[0];
        }
    }
    return nullptr;
}

TagEntryPtr CxxCodeCompletion::lookup_child_symbol(TagEntryPtr parent, const wxString& child_symbol,
                                                   const vector<wxString>& visible_scopes,
                                                   const vector<wxString>& kinds)
{
    CHECK_PTR_RET_NULL(m_lookup);
    auto resolved = lookup_symbol_by_kind(child_symbol, visible_scopes, kinds);
    if(resolved) {
        return resolved;
    }

    // try with the parent
    CHECK_PTR_RET_NULL(parent);

    // to avoid spaces and other stuff that depends on the user typing
    // we tokenize the symbol name
    vector<wxString> requested_symbol_tokens;
    {
        CxxLexerToken tok;
        CxxTokenizer tokenizer;
        tokenizer.Reset(child_symbol);
        while(tokenizer.NextToken(tok)) {
            requested_symbol_tokens.push_back(tok.GetWXString());
        }
    }

    auto compare_tokens_func = [&requested_symbol_tokens](const wxString& symbol_name) -> bool {
        CxxLexerToken tok;
        CxxTokenizer tokenizer;
        tokenizer.Reset(symbol_name);
        for(const wxString& token : requested_symbol_tokens) {
            if(!tokenizer.NextToken(tok)) {
                return false;
            }

            if(tok.GetWXString() != token)
                return false;
        }

        // if we can read more tokens from the tokenizer, it means that
        // the comparison is not complete, it "contains" the requested name
        if(tokenizer.NextToken(tok)) {
            return false;
        }
        return true;
    };

    deque<TagEntryPtr> q;
    q.push_front(parent);
    wxStringSet_t visited;
    while(!q.empty()) {
        auto t = q.front();
        q.pop_front();

        // avoid visiting the same tag twice
        if(!visited.insert(t->GetPath()).second)
            continue;

        vector<TagEntryPtr> tags;
        m_lookup->GetTagsByScope(t->GetPath(), tags);
        for(TagEntryPtr child : tags) {
            if(compare_tokens_func(child->GetName())) {
                return child;
            }
        }

        // if we got here, no match
        wxArrayString inherits = t->GetInheritsAsArrayNoTemplates();
        for(const wxString& inherit : inherits) {
            auto match = lookup_symbol_by_kind(inherit, visible_scopes, { "class", "struct" });
            if(match) {
                q.push_back(match);
            }
        }
    }
    return nullptr;
}

TagEntryPtr CxxCodeCompletion::lookup_symbol_by_kind(const wxString& name, const vector<wxString>& visible_scopes,
                                                     const vector<wxString>& kinds)
{
    vector<TagEntryPtr> tags;
    vector<wxString> scopes_to_check = visible_scopes;
    if(scopes_to_check.empty()) {
        scopes_to_check.push_back(wxEmptyString);
    }

    for(const wxString& scope : scopes_to_check) {
        wxString path;
        if(!scope.empty()) {
            path << scope << "::";
        }
        path << name;
        m_lookup->GetTagsByPathAndKind(path, tags, kinds);
        if(tags.size() == 1) {
            // we got a match
            return tags[0];
        }
    }
    return tags.empty() ? nullptr : tags[0];
}

void CxxCodeCompletion::update_template_table(TagEntryPtr resolved, CxxExpression& curexpr,
                                              const vector<wxString>& visible_scopes, wxStringSet_t& visited)
{
    CHECK_PTR_RET(resolved);
    if(!visited.insert(resolved->GetPath()).second) {
        // already visited this node
        return;
    }

    // simple template instantiaion line
    wxString pattern = normalize_pattern(resolved);
    if(curexpr.is_template()) {
        curexpr.parse_template_placeholders(pattern);
        wxStringMap_t M = curexpr.get_template_placeholders_map();
        m_template_manager->add_placeholders(M, visible_scopes);
    }

    // Check if one of the parents is a template class
    vector<wxString> inhertiance_expressions = CxxExpression::split_subclass_expression(normalize_pattern(resolved));
    for(const wxString& inherit : inhertiance_expressions) {
        vector<CxxExpression> more_expressions = CxxExpression::from_expression(inherit + ".", nullptr);
        if(more_expressions.empty())
            continue;

        auto match = lookup_symbol_by_kind(more_expressions[0].type_name(), visible_scopes, { "class", "struct" });
        if(match) {
            update_template_table(match, more_expressions[0], visible_scopes, visited);
        }
    }
}

TagEntryPtr CxxCodeCompletion::lookup_symbol(CxxExpression& curexpr, const vector<wxString>& visible_scopes,
                                             TagEntryPtr parent)
{
    wxString name_to_find = curexpr.type_name();
    auto resolved_name = m_template_manager->resolve(name_to_find, visible_scopes);
    if(resolved_name != name_to_find) {
        name_to_find = resolved_name;
        auto expressions = CxxExpression::from_expression(name_to_find + curexpr.operand_string(), nullptr);
        return resolve_compound_expression(expressions, visible_scopes, curexpr);
    }

    // try classes first
    auto resolved = lookup_child_symbol(parent, name_to_find, visible_scopes,
                                        { "typedef", "class", "struct", "namespace", "cenum", "enum", "union" });
    if(!resolved) {
        // try methods
        // `lookup_child_symbol` takes inheritance into consideration
        resolved = lookup_child_symbol(parent, name_to_find, visible_scopes,
                                       { "function", "prototype", "member", "enumerator" });
    }

    if(resolved) {
        // update the template table
        wxStringSet_t visited;
        update_template_table(resolved, curexpr, visible_scopes, visited);

        // Check for operator-> overloading
        if(curexpr.check_subscript_operator()) {
            // we check for subscript before ->
            TagEntryPtr subscript_tag = lookup_subscript_operator(resolved, visible_scopes);
            if(subscript_tag) {
                resolved = subscript_tag;
                curexpr.pop_subscript_operator();
            }
        }

        if(curexpr.operand_string() == "->") {
            // search for operator-> overloading
            TagEntryPtr arrow_tag = lookup_operator_arrow(resolved, visible_scopes);
            if(arrow_tag) {
                resolved = arrow_tag;
                // change the operand from -> to .
                // to avoid extra resolving in case we are resolving expression like this:
                // ```
                //  shared_ptr<shared_ptr<wxString>> p;
                //  p->
                // ```
                // without changing the operand, we will get completions for `wxString`
                curexpr.set_operand('.');
            }
        }
    }
    return resolved;
}

vector<wxString> CxxCodeCompletion::update_visible_scope(const vector<wxString>& curscopes, TagEntryPtr tag)
{
    vector<wxString> scopes;
    scopes.insert(scopes.end(), curscopes.begin(), curscopes.end());

    if(tag && (tag->IsClass() || tag->IsStruct() || tag->IsNamespace() || tag->GetKind() == "union")) {
        prepend_scope(scopes, tag->GetPath());
    } else if(tag && tag->IsMethod()) {
        prepend_scope(scopes, tag->GetScope());
    }
    return scopes;
}

TagEntryPtr CxxCodeCompletion::resolve_expression(CxxExpression& curexp, TagEntryPtr parent,
                                                  const vector<wxString>& visible_scopes)
{
    // test locals first, if its empty, its the first time we are entering here
    if(m_first_time && !parent) {
        if(curexp.is_this()) {
            // this can only work with ->
            if(curexp.operand_string() != "->") {
                return nullptr;
            }

            // replace "this" with the current scope name
            determine_current_scope();
            wxString exprstr = m_current_scope_name + curexp.operand_string();
            vector<CxxExpression> expr_arr = CxxExpression::from_expression(exprstr, nullptr);
            return resolve_compound_expression(expr_arr, visible_scopes, curexp);

        } else if(curexp.operand_string() == "." || curexp.operand_string() == "->") {
            if(m_locals.count(curexp.type_name())) {
                wxString exprstr = m_locals.find(curexp.type_name())->second.type_name() + curexp.operand_string();
                vector<CxxExpression> expr_arr = CxxExpression::from_expression(exprstr, nullptr);
                return resolve_compound_expression(expr_arr, visible_scopes, curexp);
            } else {
                determine_current_scope();

                // try to load the symbol this order:
                // current-scope::name (if we are in a scope)
                // other-scopes::name
                // global-scope::name
                vector<wxString> paths_to_try;
                if(!m_current_scope_name.empty()) {
                    paths_to_try.push_back(m_current_scope_name + "::" + curexp.type_name());
                }

                // add the other scopes
                for(auto scope : visible_scopes) {
                    wxString path_to_try;
                    if(!scope.empty()) {
                        path_to_try << scope << "::";
                    }
                    path_to_try << curexp.type_name();
                    paths_to_try.push_back(path_to_try);
                }

                for(const auto& path_to_try : paths_to_try) {
                    auto scope_tag = lookup_symbol_by_kind(path_to_try, visible_scopes,
                                                           { "class", "struct", "union", "prototype", "function" });
                    if(scope_tag) {
                        // we are inside a scope, use the scope as the parent
                        // and call resolve_expression() again
                        return resolve_expression(curexp, scope_tag, visible_scopes);
                    }
                }
            }
        }
    }

    // update the visible scopes
    vector<wxString> scopes = update_visible_scope(visible_scopes, parent);

    auto resolved = lookup_symbol(curexp, scopes, parent);
    CHECK_PTR_RET_NULL(resolved);

    if(resolved->IsContainer()) {
        // nothing more to be done here
        return resolved;
    }

    // continue only if one of 3: member, method or typedef
    if(!resolved->IsMethod() && !resolved->IsMember() && !resolved->IsTypedef()) {
        return nullptr;
    }

    // after we resolved the expression, update the scope
    scopes = update_visible_scope(scopes, resolved);

    if(resolved->IsMethod()) {
        // parse the return value
        wxString new_expr = get_return_value(resolved) + curexp.operand_string();
        vector<CxxExpression> expr_arr = CxxExpression::from_expression(new_expr, nullptr);
        return resolve_compound_expression(expr_arr, scopes, curexp);
    } else if(resolved->IsTypedef()) {
        // substitude the type with the typeref
        wxString new_expr;
        if(!resolve_user_type(resolved->GetPath(), visible_scopes, &new_expr)) {
            new_expr = typedef_from_tag(resolved);
        }
        new_expr += curexp.operand_string();
        vector<CxxExpression> expr_arr = CxxExpression::from_expression(new_expr, nullptr);
        return resolve_compound_expression(expr_arr, scopes, curexp);
    } else if(resolved->IsMember()) {
        // replace the member variable by its type
        unordered_map<wxString, __local> locals_variables;
        if((parse_locals(normalize_pattern(resolved), &locals_variables) == 0) ||
           (locals_variables.count(resolved->GetName()) == 0)) {
            return nullptr;
        }

        wxString new_expr = locals_variables[resolved->GetName()].type_name() + curexp.operand_string();
        vector<CxxExpression> expr_arr = CxxExpression::from_expression(new_expr, nullptr);
        return resolve_compound_expression(expr_arr, scopes, curexp);
    }
    return nullptr;
}

const wxStringMap_t& CxxCodeCompletion::get_tokens_map() const { return m_macros_table_map; }

wxString CxxCodeCompletion::get_return_value(TagEntryPtr tag) const
{
    clFunction f;
    wxString pattern = normalize_pattern(tag);
    tag->SetPattern("/^ " + pattern + " $/");
    if(LanguageST::Get()->FunctionFromPattern(tag, f)) {
        wxString return_value;
        if(!f.m_returnValue.m_typeScope.empty()) {
            return_value << f.m_returnValue.m_typeScope << "::";
        }
        return_value << f.m_returnValue.m_type;
        return return_value;
    }
    return wxEmptyString;
}

void CxxCodeCompletion::prepend_scope(vector<wxString>& scopes, const wxString& scope) const
{
    auto where = find_if(scopes.begin(), scopes.end(), [=](const wxString& s) { return s == scope; });

    if(where != scopes.end()) {
        scopes.erase(where);
    }

    scopes.insert(scopes.begin(), scope);
}

void CxxCodeCompletion::reset()
{
    m_locals.clear();
    m_optimized_scope.clear();
    m_template_manager->clear();
    m_recurse_protector = 0;
    m_current_scope_name.clear();
}

wxString CxxCodeCompletion::typedef_from_tag(TagEntryPtr tag) const
{
    wxString typedef_str;
    CxxTokenizer tkzr;
    CxxLexerToken tk;

    if(!tag->GetTyperef().empty()) {
        typedef_str = tag->GetTyperef();
        return typedef_str.Trim();
    }

    wxString pattern = normalize_pattern(tag);
    tkzr.Reset(pattern);

    vector<wxString> V;
    wxString last_identifier;
    // consume the first token which is one of T_USING | T_TYPEDEF
    tkzr.NextToken(tk);

    bool parse_succeeded = false;
    if(tk.GetType() == T_USING) {
        // "using" syntax:
        // using element_type = _Tp;
        // read until the "=";

        // consume everything until we find the "="
        while(tkzr.NextToken(tk)) {
            if(tk.GetType() == '=')
                break;
        }

        // read until the ";"
        while(tkzr.NextToken(tk)) {
            if(tk.GetType() == ';') {
                parse_succeeded = true;
                break;
            }
            if(tk.is_keyword()) {
                // dont pick keywords
                continue;
            }
            if(tk.is_builtin_type()) {
                V.push_back((V.empty() ? "" : " ") + tk.GetWXString() + " ");
            } else {
                V.push_back(tk.GetWXString());
            }
        }

    } else if(tk.GetType() == T_TYPEDEF) {
        // standard "typedef" syntax:
        // typedef wxString MyString;
        while(tkzr.NextToken(tk)) {
            if(tk.is_keyword()) {
                continue;
            }
            if(tk.GetType() == ';') {
                // end of typedef
                if(!V.empty()) {
                    V.pop_back();
                }
                parse_succeeded = true;
                break;
            } else {
                if(tk.is_builtin_type()) {
                    V.push_back((V.empty() ? "" : " ") + tk.GetWXString() + " ");
                } else {
                    V.push_back(tk.GetWXString());
                }
            }
        }
    }

    if(!parse_succeeded) {
        return wxEmptyString;
    } else {
        for(const wxString& s : V) {
            typedef_str << s;
        }
    }
    return typedef_str.Trim();
}

vector<TagEntryPtr> CxxCodeCompletion::get_locals(const wxString& filter) const
{
    vector<TagEntryPtr> locals;
    locals.reserve(m_locals.size());

    wxString lowercase_filter = filter.Lower();
    for(const auto& vt : m_locals) {
        const auto& local = vt.second;
        TagEntryPtr tag(new TagEntry());
        tag->SetName(local.name());
        tag->SetKind("variable");
        tag->SetParent("<local>");
        tag->SetScope(local.type_name());
        tag->SetAccess("public");
        tag->SetPattern("/^ " + local.pattern() + " $/");

        if(!tag->GetName().Lower().StartsWith(lowercase_filter))
            continue;
        locals.push_back(tag);
    }
    return locals;
}

size_t CxxCodeCompletion::get_completions(TagEntryPtr parent, const wxString& operand_string, const wxString& filter,
                                          vector<TagEntryPtr>& candidates, const vector<wxString>& visible_scopes,
                                          size_t limit)
{
    if(!parent) {
        return 0;
    }

    vector<wxString> kinds = { "function", "prototype", "member", "enum",      "enumerator",
                               "class",    "struct",    "union",  "namespace", "typedef" };
    // the global scope
    candidates = get_children_of_scope(parent, kinds, filter, visible_scopes);
    wxStringSet_t visited;

    vector<TagEntryPtr> unique_candidates;
    unique_candidates.reserve(candidates.size());
    for(TagEntryPtr tag : candidates) {
        if(!visited.insert(tag->GetPath()).second) {
            // already seen this
            continue;
        }
        unique_candidates.emplace_back(tag);
    }

    // sort the matches
    auto sort_cb = [=](TagEntryPtr a, TagEntryPtr b) { return a->GetName().CmpNoCase(b->GetName()) < 0; };
    sort(unique_candidates.begin(), unique_candidates.end(), sort_cb);
    // truncate the list to match the limit
    if(unique_candidates.size() > limit) {
        unique_candidates.resize(limit);
    }
    candidates.swap(unique_candidates);

    // filter the results
    vector<TagEntryPtr> sorted_tags;
    sort_tags(candidates, sorted_tags, false, {});
    candidates.swap(sorted_tags);
    return candidates.size();
}

vector<TagEntryPtr> CxxCodeCompletion::get_scopes(TagEntryPtr parent, const vector<wxString>& visible_scopes)
{
    vector<TagEntryPtr> scopes;
    scopes.reserve(100);

    deque<TagEntryPtr> q;
    q.push_front(parent);
    wxStringSet_t visited;
    while(!q.empty()) {
        auto t = q.front();
        q.pop_front();

        // avoid visiting the same tag twice
        if(!visited.insert(t->GetPath()).second)
            continue;

        scopes.push_back(t);

        // if we got here, no match
        wxArrayString inherits = t->GetInheritsAsArrayNoTemplates();
        for(const wxString& inherit : inherits) {
            auto match = lookup_symbol_by_kind(inherit, visible_scopes, { "class", "struct" });
            if(match) {
                q.push_back(match);
            }
        }
    }
    return scopes;
}

vector<TagEntryPtr> CxxCodeCompletion::get_children_of_scope(TagEntryPtr parent, const vector<wxString>& kinds,
                                                             const wxString& filter,
                                                             const vector<wxString>& visible_scopes)
{
    if(!m_lookup) {
        return {};
    }

    vector<TagEntryPtr> tags;
    wxArrayString wx_kinds;
    wx_kinds.reserve(kinds.size());
    for(const wxString& kind : kinds) {
        wx_kinds.Add(kind);
    }

    auto parents = get_scopes(parent, visible_scopes);
    for(auto tag : parents) {
        wxString scope = tag->GetPath();
        if(tag->IsMethod()) {
            scope = tag->GetScope();
        }
        vector<TagEntryPtr> parent_tags;
        m_lookup->GetTagsByScopeAndKind(scope, wx_kinds, filter, parent_tags, true);
        tags.reserve(tags.size() + parent_tags.size());
        tags.insert(tags.end(), parent_tags.begin(), parent_tags.end());
    }
    return tags;
}

void CxxCodeCompletion::set_text(const wxString& text, const wxString& filename, int current_line)
{
    m_optimized_scope.clear();
    m_locals.clear();
    m_optimized_scope = shrink_scope(text, &m_locals);
    m_filename = filename;
    m_line_number = current_line;
    m_current_scope_name.clear();

    if(!m_filename.empty() && m_line_number != wxNOT_FOUND) {
        determine_current_scope();
    }
}

namespace
{

bool find_wild_match(const vector<pair<wxString, wxString>>& table, const wxString& find_what, wxString* match)
{
    for(const auto& p : table) {
        // we support wildcard matching
        if(::wxMatchWild(p.first, find_what)) {
            *match = p.second;
            return true;
        }
    }
    return false;
}

bool try_resovle_user_type_with_scopes(const vector<pair<wxString, wxString>>& table, const wxString& type,
                                       const vector<wxString>& visible_scopes, wxString* resolved)
{
    for(const wxString& scope : visible_scopes) {
        wxString user_type = scope;
        if(!user_type.empty()) {
            user_type << "::";
        }
        user_type << type;
        if(find_wild_match(table, type, resolved)) {
            return true;
        }
    }
    return false;
}
}; // namespace

bool CxxCodeCompletion::resolve_user_type(const wxString& type, const vector<wxString>& visible_scopes,
                                          wxString* resolved) const
{
    bool match_found = false;
    wxStringSet_t visited;
    *resolved = type;
    while(true) {
        if(!visited.insert(*resolved).second) {
            // already tried this type
            break;
        }

        if(!try_resovle_user_type_with_scopes(m_types_table, *resolved, visible_scopes, resolved)) {
            break;
        }
        match_found = true;
    }

    if(match_found) {
        return true;
    }
    return false;
}

void TemplateManager::clear() { m_table.clear(); }

void TemplateManager::add_placeholders(const wxStringMap_t& table, const vector<wxString>& visible_scopes)
{
    // try to resolve any of the template before we insert them
    // its important to do it now so we use the correct scope
    wxStringMap_t M;
    for(const auto& vt : table) {
        wxString name = vt.first;
        wxString value;

        auto resolved = m_completer->lookup_child_symbol(
            nullptr, vt.second, visible_scopes,
            { "class", "struct", "typedef", "union", "namespace", "enum", "enumerator" });
        if(resolved) {
            // use the path found in the db
            value = resolved->GetPath();
        } else {
            // lets try and avoid pushing values that are templates
            // consider
            // template <typename _Tp> class vector : protected _Vector_base<_Tp> {..}
            // Looking at the definitio of _Vector_base:
            // template <typename _Tp> class _Vector_base {...}
            // this will cause us to push {"_Tp", "_Tp"} (where _Tp is both the key and value)
            // if the resolve will fail, it will return vt.second unmodified
            value = this->resolve(vt.second, visible_scopes);
        }
        M.insert({ name, value });
    }
    m_table.insert(m_table.begin(), M);
}

#define STRIP_PLACEHOLDER(__ph)                        \
    stripped_placeholder.Replace("*", wxEmptyString);  \
    stripped_placeholder.Replace("->", wxEmptyString); \
    stripped_placeholder.Replace("&&", wxEmptyString);

namespace
{
bool try_resolve_placeholder(const wxStringMap_t& table, const wxString& name, wxString* resolved)
{
    // strip operands from `s`
    wxString stripped_placeholder = name;
    STRIP_PLACEHOLDER(stripped_placeholder);

    if(table.count(name)) {
        *resolved = table.find(name)->second;
        return true;
    }
    return false;
}
}; // namespace

wxString TemplateManager::resolve(const wxString& name, const vector<wxString>& visible_scopes) const
{
    wxStringSet_t visited;
    wxString resolved = name;
    for(const wxStringMap_t& table : m_table) {
        try_resolve_placeholder(table, resolved, &resolved);
    }
    return resolved;
}

void CxxCodeCompletion::set_macros_table(const vector<pair<wxString, wxString>>& t)
{
    m_macros_table = t;
    m_macros_table_map.reserve(m_macros_table.size());
    for(const auto& d : m_macros_table) {
        m_macros_table_map.insert(d);
    }
}

void CxxCodeCompletion::sort_tags(const vector<TagEntryPtr>& tags, vector<TagEntryPtr>& sorted_tags,
                                  bool include_ctor_dtor, const wxStringSet_t& visible_files)
{
    TagEntryPtrVector_t publicTags;
    TagEntryPtrVector_t protectedTags;
    TagEntryPtrVector_t privateTags;
    TagEntryPtrVector_t locals;
    TagEntryPtrVector_t members;

    bool skip_tor = !include_ctor_dtor;

    // first: remove duplicates
    unordered_set<int> visited_by_id;
    unordered_set<wxString> visited_by_name;

    for(size_t i = 0; i < tags.size(); ++i) {
        TagEntryPtr tag = tags.at(i);

        // only include matches from the provided list of files
        if(!visible_files.empty() && visible_files.count(tag->GetFile()) == 0)
            continue;

        if(skip_tor && (tag->IsConstructor() || tag->IsDestructor()))
            continue;

        bool is_local = tag->IsLocalVariable() || tag->GetScope() == "<local>";
        // we filter local tags by name
        if(is_local && !visited_by_name.insert(tag->GetName()).second) {
            continue;
        }

        // other tags (loaded from the db) are filtered by their ID
        if(!is_local && !visited_by_id.insert(tag->GetId()).second) {
            continue;
        }

        // filter by type/access
        wxString access = tag->GetAccess();
        wxString kind = tag->GetKind();

        if(kind == "variable") {
            locals.push_back(tag);

        } else if(kind == "member") {
            members.push_back(tag);

        } else if(access == "private") {
            privateTags.push_back(tag);

        } else if(access == "protected") {
            protectedTags.push_back(tag);

        } else if(access == "public") {
            if(tag->GetName().StartsWith("_") || tag->GetName().Contains("operator")) {
                // methods starting with _ usually are meant to be private
                // and also, put the "operator" methdos at the bottom
                privateTags.push_back(tag);
            } else {
                publicTags.push_back(tag);
            }
        } else {
            // assume private
            privateTags.push_back(tag);
        }
    }

    auto sort_func = [=](TagEntryPtr a, TagEntryPtr b) { return a->GetName().CmpNoCase(b->GetName()) < 0; };

    sort(privateTags.begin(), privateTags.end(), sort_func);
    sort(publicTags.begin(), publicTags.end(), sort_func);
    sort(protectedTags.begin(), protectedTags.end(), sort_func);
    sort(members.begin(), members.end(), sort_func);
    sort(locals.begin(), locals.end(), sort_func);

    sorted_tags.clear();
    sorted_tags.insert(sorted_tags.end(), locals.begin(), locals.end());
    sorted_tags.insert(sorted_tags.end(), publicTags.begin(), publicTags.end());
    sorted_tags.insert(sorted_tags.end(), protectedTags.begin(), protectedTags.end());
    sorted_tags.insert(sorted_tags.end(), privateTags.begin(), privateTags.end());
    sorted_tags.insert(sorted_tags.end(), members.begin(), members.end());
}

size_t CxxCodeCompletion::get_file_completions(const wxString& user_typed, vector<TagEntryPtr>& files,
                                               const wxString& suffix)
{
    if(!m_lookup) {
        return 0;
    }
    wxArrayString files_arr;
    m_lookup->GetFilesForCC(user_typed, files_arr);

    files.reserve(files_arr.size());
    for(const wxString& file : files_arr) {
        // exclude source file
        if(FileExtManager::GetType(file) == FileExtManager::TypeSourceC ||
           FileExtManager::GetType(file) == FileExtManager::TypeSourceCpp) {
            continue;
        }
        TagEntryPtr tag(new TagEntry());

        wxString display_name = file + suffix;
        tag->SetKind("file");
        tag->SetName(display_name); // display name
        display_name = display_name.AfterLast('/');
        tag->SetPattern(display_name); // insert text
        tag->SetLine(-1);
        files.push_back(tag);
    }
    return files.size();
}

size_t CxxCodeCompletion::get_children_of_current_scope(const vector<wxString>& kinds, const wxString& filter,
                                                        const vector<wxString>& visible_scopes,
                                                        vector<TagEntryPtr>* current_scope_children,
                                                        vector<TagEntryPtr>* other_scopes_children,
                                                        vector<TagEntryPtr>* global_scope_children)
{
    auto resolved = determine_current_scope();
    if(resolved) {
        *current_scope_children = get_children_of_scope(resolved, kinds, filter, visible_scopes);
    }

    // collect "other scopes"
    wxArrayString wx_kinds = to_wx_array_string(kinds);
    wxArrayString wx_scope = to_wx_array_string(visible_scopes);
    m_lookup->GetTagsByScopeAndName(wx_scope, filter, true, *other_scopes_children);

    // global scope children
    *global_scope_children = get_children_of_scope(create_global_scope_tag(), kinds, filter, visible_scopes);
    return current_scope_children->size() + other_scopes_children->size() + global_scope_children->size();
}

size_t CxxCodeCompletion::get_word_completions(const CxxRemainder& remainder, vector<TagEntryPtr>& candidates,
                                               const vector<wxString>& visible_scopes,
                                               const wxStringSet_t& visible_files)

{
    vector<TagEntryPtr> locals;
    vector<TagEntryPtr> scope_members;
    vector<TagEntryPtr> other_scopes_members;
    vector<TagEntryPtr> global_scopes_members;

    vector<TagEntryPtr> sorted_locals;
    vector<TagEntryPtr> sorted_scope_members;
    vector<TagEntryPtr> sorted_other_scopes_members;
    vector<TagEntryPtr> sorted_global_scopes_members;

    locals = get_locals(remainder.filter);
    vector<wxString> kinds;
    // based on the lasts operand, build the list of items to fetch
    auto current_scope = determine_current_scope();
    if(remainder.operand_string.empty()) {
        kinds = { "function", "prototype", "class",      "struct", "namespace", "union",
                  "typedef",  "enum",      "enumerator", "macro",  "cenum" };
        if(current_scope) {
            // if we are inside a scope, add member types
            kinds.push_back("member");
        }
    } else if(remainder.operand_string == "::") {
        kinds = { "member",    "function", "prototype", "class", "struct",
                  "namespace", "union",    "typedef",   "enum",  "enumerator" };
    } else {
        // contextual completions
        kinds = { "member", "function", "prototype" };
    }

    // collect member variabels if we are within a scope
    get_children_of_current_scope(kinds, remainder.filter, visible_scopes, &scope_members, &other_scopes_members,
                                  &global_scopes_members);

    // sort the matches:
    sort_tags(locals, sorted_locals, true, {});               // locals are accepted, so dont pass list of files
    sort_tags(scope_members, sorted_scope_members, true, {}); // members are all accepted
    sort_tags(other_scopes_members, sorted_other_scopes_members, true, visible_files);
    sort_tags(global_scopes_members, sorted_global_scopes_members, true, visible_files);

    candidates.reserve(sorted_locals.size() + sorted_scope_members.size() + sorted_other_scopes_members.size() +
                       sorted_global_scopes_members.size());
    candidates.insert(candidates.end(), sorted_locals.begin(), sorted_locals.end());
    candidates.insert(candidates.end(), sorted_scope_members.begin(), sorted_scope_members.end());
    candidates.insert(candidates.end(), sorted_other_scopes_members.begin(), sorted_other_scopes_members.end());
    candidates.insert(candidates.end(), sorted_global_scopes_members.begin(), sorted_global_scopes_members.end());
    return candidates.size();
}

TagEntryPtr CxxCodeCompletion::find_definition(const wxString& filepath, int line, const wxString& expression,
                                               const wxString& text, const vector<wxString>& visible_scopes)
{
    // ----------------------------------
    // word completion
    // ----------------------------------
    clDEBUG() << "find_definition is called" << endl;
    vector<TagEntryPtr> candidates;
    if(word_complete(filepath, line, expression, text, visible_scopes, true, candidates) == 0) {
        return nullptr;
    }

    // filter tags with no line numbers
    vector<TagEntryPtr> good_tags;
    TagEntryPtr first;
    for(auto tag : candidates) {
        if(tag->GetLine() != wxNOT_FOUND && !tag->GetFile().empty()) {
            first = tag;
            break;
        }
    }

    if(!first) {
        return nullptr;
    }

    if(first->IsMethod()) {
        // we prefer the definition, unless we are already on it, and in that case, return the declaration
        // locate both declaration + implementation
        wxString path = first->GetPath();
        vector<TagEntryPtr> impl_vec;
        vector<TagEntryPtr> decl_vec;
        clDEBUG() << "Searching for path:" << path << endl;
        m_lookup->GetTagsByPathAndKind(path, impl_vec, { "function" }, 1);
        m_lookup->GetTagsByPathAndKind(path, decl_vec, { "prototype" }, 1);

        clDEBUG() << "impl:" << impl_vec.size() << "decl:" << decl_vec.size() << endl;
        if(impl_vec.empty() || decl_vec.empty()) {
            return first;
        }

        TagEntryPtr impl = impl_vec[0];
        TagEntryPtr decl = decl_vec[0];

        if(impl->GetLine() == line && impl->GetFile() == filepath) {
            // already on the impl line, return the decl
            return decl;
        }

        if(decl->GetLine() == line && decl->GetFile() == filepath) {
            // already on the decl line, return the decl
            return impl;
        }

        return impl;
    } else {
        // no need to manipulate this tag
        return first;
    }
}

size_t CxxCodeCompletion::word_complete(const wxString& filepath, int line, const wxString& expression,
                                        const wxString& text, const vector<wxString>& visible_scopes, bool exact_match,
                                        vector<TagEntryPtr>& candidates, const wxStringSet_t& visible_files)
{
    // ----------------------------------
    // word completion
    // ----------------------------------
    clDEBUG() << "find_definition expression:" << expression << endl;
    set_text(text, filepath, line);

    CxxRemainder remainder;
    TagEntryPtr resolved = code_complete(expression, visible_scopes, &remainder);

    wxString filter = remainder.filter;
    if(!resolved) {
        // check if this is a contextual text (A::B or a->b or a.b etc)
        // we only allow global completion of there is not expression list
        // and the remainder has something in it (r.type_name() is not empty)
        CxxRemainder r;
        auto expressions = CxxExpression::from_expression(expression, &r);
        if(expressions.size() == 0 && !r.filter.empty()) {
            clDEBUG() << "code_complete failed to resolve:" << expression << endl;
            clDEBUG() << "filter:" << r.filter << endl;
            get_word_completions(remainder, candidates, visible_scopes, visible_files);
        }
    } else if(resolved) {
        // it was resolved into something...
        clDEBUG() << "code_complete resolved:" << resolved->GetPath() << endl;
        clDEBUG() << "filter:" << remainder.filter << endl;
        get_completions(resolved, remainder.operand_string, remainder.filter, candidates, visible_scopes);
    }
    clDEBUG() << "Number of completion entries:" << candidates.size() << endl;

    // take the first one with the exact match
    if(!exact_match) {
        return candidates.size();
    }

    vector<TagEntryPtr> matches;
    matches.reserve(candidates.size());
    for(TagEntryPtr t : candidates) {
        if(t->GetName() == filter) {
            matches.push_back(t);
        }
    }
    candidates.swap(matches);
    return candidates.size();
}

wxString CxxCodeCompletion::normalize_pattern(TagEntryPtr tag) const
{
    CxxTokenizer tokenizer;
    CxxLexerToken tk;

    tokenizer.Reset(tag->GetPatternClean());
    wxString pattern;
    while(tokenizer.NextToken(tk)) {
        wxString str = tk.GetWXString();
        switch(tk.GetType()) {
        case T_IDENTIFIER:
            if(m_macros_table_map.count(str) && m_macros_table_map.find(str)->second.empty()) {
                // skip this token
            } else {
                pattern << str << " ";
            }
            break;
        default:
            if(tk.is_keyword() || tk.is_builtin_type()) {
                pattern << str << " ";
            } else {
                pattern << str;
            }
            break;
        }
    }
    return pattern;
}
