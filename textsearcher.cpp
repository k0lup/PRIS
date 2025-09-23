#include "textsearcher.h"

static QTextCharFormat makeFormat(QColor bg, QColor fg = Qt::black){
    QTextCharFormat f;
    f.setBackground(bg);
    f.setForeground(fg);
    return f;
}

const static QTextCharFormat fmtAll = makeFormat("#fff59d");
const static QTextCharFormat fmtCur = makeFormat(QColor("#ffab40"));

TextSearcher::TextSearcher(QTextEdit *edit, QObject *parent) : QObject(parent), m_edit(edit)
{

}

QTextDocument::FindFlags TextSearcher::makeFindFlags() const{
    QTextDocument::FindFlags f;
    if (m_cs == Qt::CaseSensitivity::CaseSensitive) f |= QTextDocument::FindCaseSensitively;
    return f;
}

void TextSearcher::setSearchTerm(const QString &term, Qt::CaseSensitivity cs)
{
    m_term = term;
    m_cs = cs;
    highlightAll();
}

void TextSearcher::highlightAll()
{
    m_extra.clear();
    m_current = QTextCursor();
    if (m_term.isEmpty()){
        m_edit->setExtraSelections({});
        return;
    }

    QTextDocument *doc = m_edit->document();
    QTextCursor c(doc);
    //QTextCharFormat fmtAll = makeFormat(QColor("#fff59d"));
    //QTextCharFormat fmtCurr = makeFormat(QColor("#ffab40"));

    //m_cs не принимает
    while (!(c = doc->find(m_term, c, makeFindFlags())).isNull()){
        QTextEdit::ExtraSelection sel;
        sel.cursor = c;
        sel.format = fmtAll;
        m_extra.append(sel);
    }

    if (!m_extra.isEmpty()){
        m_current = m_extra.first().cursor;
        ensureVisible(m_current);
        m_extra.first().format = fmtCur;
    }

    m_edit->setExtraSelections(m_extra);
}

void TextSearcher::ensureVisible(const QTextCursor &c){
    //m_edit->setTextCursor(c);
    //m_edit->ensureCursorVisible();

    QTextCursor caret(c);
    caret.setPosition((c.selectionStart()));

    m_edit->setTextCursor(caret);
    m_edit->ensureCursorVisible();
}

void TextSearcher::feedbackNotFound(bool backwards){
    const QString msg = QString("Больше нет совпадений %1").arg(backwards ? "выше" : "ниже");
    QMessageBox::information(m_edit, "Поиск", msg);
}

void TextSearcher::findNext(){
    if (m_term.isEmpty()) return;

    QTextDocument::FindFlags dir = makeFindFlags();
    QTextCursor res = m_edit->document()->find(m_term, m_current, dir);

    if (res.isNull()){
        feedbackNotFound(false);
        return;
    }

    //QTextCharFormat fmtAll = makeFormat("#fff59d");
    //QTextCharFormat fmtCur = makeFormat("#ffab40");
    for (auto &sel : m_extra){
        if (sel.cursor == m_current)
            sel.format = fmtAll;
        else if (sel.cursor == res)
            sel.format = fmtCur;
    }

    m_current = res;
    m_edit->setExtraSelections(m_extra);
    ensureVisible(m_current);
    /*for (auto &sel : m_extra){
        if (sel.cursor == res)
            sel.format = fmtCur;
    }*/
}

void TextSearcher::findPrevious(){
    if (m_term.isEmpty()){
        return;
    }

    QTextDocument::FindFlags dir = makeFindFlags() | QTextDocument::FindBackward;
    QTextCursor res = m_edit->document()->find(m_term, m_current, dir);

    if (res.isNull()){
        feedbackNotFound(true);
        return;
    }

    //QTextCharFormat fmtAll = makeFormat("#fff59d");
    //QTextCharFormat fmtCur = makeFormat("#ffab40");
    for (auto &sel : m_extra){
        if (sel.cursor == m_current)
            sel.format = fmtAll;
        else if (sel.cursor == res){
            sel.format = fmtCur;
        }
    }

    m_current = res;
    m_edit->setExtraSelections(m_extra);
    ensureVisible(m_current);

    /*for (auto &sel : m_extra){
        if (sel.cursor == res)
            sel.format = fmtCur;
    }*/
}
