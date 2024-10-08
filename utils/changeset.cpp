// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "changeset.h"

#include <QTextCursor>

namespace Utils {

ChangeSet::ChangeSet()
    : m_string(nullptr), m_cursor(nullptr), m_error(false)
{
}

ChangeSet::ChangeSet(const QList<EditOp> &operations)
    : m_string(nullptr), m_cursor(nullptr), m_operationList(operations), m_error(false)
{
}

static bool overlaps(int posA, int lengthA, int posB, int lengthB) {
    if (lengthB > 0) {
        return
                // right edge of B contained in A
                (posA < posB + lengthB && posA + lengthA >= posB + lengthB)
                // left edge of B contained in A
                || (posA <= posB && posA + lengthA > posB)
                // A contained in B
                || (posB < posA && posB + lengthB > posA + lengthA);
    } else {
        return (posB > posA && posB < posA + lengthA);
    }
}

bool ChangeSet::hasOverlap(int pos, int length) const
{
    for (const EditOp &cmd : m_operationList) {

        switch (cmd.type) {
        case EditOp::Replace:
            if (overlaps(pos, length, cmd.pos1, cmd.length1))
                return true;
            break;

        case EditOp::Move:
            if (overlaps(pos, length, cmd.pos1, cmd.length1))
                return true;
            if (cmd.pos2 > pos && cmd.pos2 < pos + length)
                return true;
            break;

        case EditOp::Insert:
            if (cmd.pos1 > pos && cmd.pos1 < pos + length)
                return true;
            break;

        case EditOp::Remove:
            if (overlaps(pos, length, cmd.pos1, cmd.length1))
                return true;
            break;

        case EditOp::Flip:
            if (overlaps(pos, length, cmd.pos1, cmd.length1))
                return true;
            if (overlaps(pos, length, cmd.pos2, cmd.length2))
                return true;
            break;

        case EditOp::Copy:
            if (overlaps(pos, length, cmd.pos1, cmd.length1))
                return true;
            if (cmd.pos2 > pos && cmd.pos2 < pos + length)
                return true;
            break;

        case EditOp::Unset:
            break;
        }
    }

    return false;
}

bool ChangeSet::isEmpty() const
{
    return m_operationList.isEmpty();
}

QList<ChangeSet::EditOp> ChangeSet::operationList() const
{
    return m_operationList;
}

void ChangeSet::clear()
{
    m_string = nullptr;
    m_cursor = nullptr;
    m_operationList.clear();
    m_error = false;
}

bool ChangeSet::replace_helper(int pos, int length, const QString &replacement)
{
    if (hasOverlap(pos, length))
        m_error = true;

    EditOp cmd(EditOp::Replace);
    cmd.pos1 = pos;
    cmd.length1 = length;
    cmd.text = replacement;
    m_operationList += cmd;

    return !m_error;
}

bool ChangeSet::move_helper(int pos, int length, int to)
{
    if (hasOverlap(pos, length)
        || hasOverlap(to, 0)
        || overlaps(pos, length, to, 0)) {
        m_error = true;
    }

    EditOp cmd(EditOp::Move);
    cmd.pos1 = pos;
    cmd.length1 = length;
    cmd.pos2 = to;
    m_operationList += cmd;

    return !m_error;
}

bool ChangeSet::insert(int pos, const QString &text)
{
    Q_ASSERT(pos >= 0);

    if (hasOverlap(pos, 0))
        m_error = true;

    EditOp cmd(EditOp::Insert);
    cmd.pos1 = pos;
    cmd.text = text;
    m_operationList += cmd;

    return !m_error;
}

bool ChangeSet::replace(const Range &range, const QString &replacement)
{ return replace(range.start, range.end, replacement); }

bool ChangeSet::remove(const Range &range)
{ return remove(range.start, range.end); }

bool ChangeSet::move(const Range &range, int to)
{ return move(range.start, range.end, to); }

bool ChangeSet::flip(const Range &range1, const Range &range2)
{ return flip(range1.start, range1.end, range2.start, range2.end); }

bool ChangeSet::copy(const Range &range, int to)
{ return copy(range.start, range.end, to); }

bool ChangeSet::replace(int start, int end, const QString &replacement)
{ return replace_helper(start, end - start, replacement); }

bool ChangeSet::remove(int start, int end)
{ return remove_helper(start, end - start); }

bool ChangeSet::move(int start, int end, int to)
{ return move_helper(start, end - start, to); }

bool ChangeSet::flip(int start1, int end1, int start2, int end2)
{ return flip_helper(start1, end1 - start1, start2, end2 - start2); }

bool ChangeSet::copy(int start, int end, int to)
{ return copy_helper(start, end - start, to); }

bool ChangeSet::remove_helper(int pos, int length)
{
    if (hasOverlap(pos, length))
        m_error = true;

    EditOp cmd(EditOp::Remove);
    cmd.pos1 = pos;
    cmd.length1 = length;
    m_operationList += cmd;

    return !m_error;
}

bool ChangeSet::flip_helper(int pos1, int length1, int pos2, int length2)
{
    if (hasOverlap(pos1, length1)
        || hasOverlap(pos2, length2)
        || overlaps(pos1, length1, pos2, length2)) {
        m_error = true;
    }

    EditOp cmd(EditOp::Flip);
    cmd.pos1 = pos1;
    cmd.length1 = length1;
    cmd.pos2 = pos2;
    cmd.length2 = length2;
    m_operationList += cmd;

    return !m_error;
}

bool ChangeSet::copy_helper(int pos, int length, int to)
{
    if (hasOverlap(pos, length)
        || hasOverlap(to, 0)
        || overlaps(pos, length, to, 0)) {
        m_error = true;
    }

    EditOp cmd(EditOp::Copy);
    cmd.pos1 = pos;
    cmd.length1 = length;
    cmd.pos2 = to;
    m_operationList += cmd;

    return !m_error;
}

void ChangeSet::doReplace(const EditOp &op, QList<EditOp> *replaceList)
{
    Q_ASSERT(op.type == EditOp::Replace);

    {
        for (EditOp &c : *replaceList) {
            if (op.pos1 <= c.pos1)
                c.pos1 += op.text.size();
            if (op.pos1 < c.pos1)
                c.pos1 -= op.length1;
        }
    }

    if (m_string) {
        m_string->replace(op.pos1, op.length1, op.text);
    } else if (m_cursor) {
        m_cursor->setPosition(op.pos1);
        m_cursor->setPosition(op.pos1 + op.length1, QTextCursor::KeepAnchor);
        m_cursor->insertText(op.text);
    }
}

void ChangeSet::convertToReplace(const EditOp &op, QList<EditOp> *replaceList)
{
    EditOp replace1(EditOp::Replace);
    EditOp replace2(EditOp::Replace);

    switch (op.type) {
    case EditOp::Replace:
        replaceList->append(op);
        break;

    case EditOp::Move:
        replace1.pos1 = op.pos1;
        replace1.length1 = op.length1;
        replaceList->append(replace1);

        replace2.pos1 = op.pos2;
        replace2.text = textAt(op.pos1, op.length1);
        replaceList->append(replace2);
        break;

    case EditOp::Insert:
        replace1.pos1 = op.pos1;
        replace1.text = op.text;
        replaceList->append(replace1);
        break;

    case EditOp::Remove:
        replace1.pos1 = op.pos1;
        replace1.length1 = op.length1;
        replaceList->append(replace1);
        break;

    case EditOp::Flip:
        replace1.pos1 = op.pos1;
        replace1.length1 = op.length1;
        replace1.text = textAt(op.pos2, op.length2);
        replaceList->append(replace1);

        replace2.pos1 = op.pos2;
        replace2.length1 = op.length2;
        replace2.text = textAt(op.pos1, op.length1);
        replaceList->append(replace2);
        break;

    case EditOp::Copy:
        replace1.pos1 = op.pos2;
        replace1.text = textAt(op.pos1, op.length1);
        replaceList->append(replace1);
        break;

    case EditOp::Unset:
        break;
    }
}

bool ChangeSet::hadErrors() const
{
    return m_error;
}

void ChangeSet::apply(QString *s)
{
    m_string = s;
    apply_helper();
    m_string = nullptr;
}

void ChangeSet::apply(QTextCursor *textCursor)
{
    m_cursor = textCursor;
    apply_helper();
    m_cursor = nullptr;
}

QString ChangeSet::textAt(int pos, int length)
{
    if (m_string) {
        return m_string->mid(pos, length);
    } else if (m_cursor) {
        m_cursor->setPosition(pos);
        m_cursor->setPosition(pos + length, QTextCursor::KeepAnchor);
        return m_cursor->selectedText();
    }
    return QString();
}

void ChangeSet::apply_helper()
{
    // convert all ops to replace
    QList<EditOp> replaceList;
    {
        while (!m_operationList.isEmpty())
            convertToReplace(m_operationList.takeFirst(), &replaceList);
    }

    // execute replaces
    if (m_cursor)
        m_cursor->beginEditBlock();

    while (!replaceList.isEmpty())
        doReplace(replaceList.takeFirst(), &replaceList);

    if (m_cursor)
        m_cursor->endEditBlock();
}

} // end namespace Utils
