#include "music_sync/planner/harmonic_compatibility.h"

#include <algorithm>
#include <cstdlib>

namespace {

int circularDistance(int a, int b, int wheel) {
    const int d = std::abs(a - b) % wheel;
    return std::min(d, wheel - d);
}

} // anonymous namespace

namespace mixxx::music_sync {

CamelotCode HarmonicCompatibility::parse(const QString& camelot) {
    CamelotCode code;
    const QString s = camelot.trimmed().toUpper();
    if (s.size() < 2) {
        return code;
    }
    const QChar last = s.at(s.size() - 1);
    if (last != QChar('A') && last != QChar('B')) {
        return code;
    }
    bool ok = false;
    const int number = s.left(s.size() - 1).toInt(&ok);
    if (!ok || number < 1 || number > 12) {
        return code;
    }
    code.number = number;
    code.letter = last.toLatin1();
    return code;
}

double HarmonicCompatibility::score(const QString& camelotA, const QString& camelotB) {
    const CamelotCode a = parse(camelotA);
    const CamelotCode b = parse(camelotB);
    if (!a.isValid() || !b.isValid()) {
        return 0.3; // unknown key -> neutral-low, never blocking
    }
    const int dist = circularDistance(a.number, b.number, 12);
    const bool sameLetter = (a.letter == b.letter);
    if (dist == 0 && sameLetter) {
        return 1.0; // same code
    }
    if (dist == 1 && sameLetter) {
        return 0.9; // adjacent on the wheel
    }
    if (dist == 0 && !sameLetter) {
        return 0.75; // relative major/minor
    }
    if (dist == 1 && !sameLetter) {
        return 0.5; // diagonal / energy shift
    }
    if (dist == 2 && sameLetter) {
        return 0.5; // two steps
    }
    return 0.2; // distant -> low, still not blocking
}

QString HarmonicCompatibility::moveLabel(const QString& camelotA, const QString& camelotB) {
    const CamelotCode a = parse(camelotA);
    const CamelotCode b = parse(camelotB);
    const QString left = a.isValid() ? camelotA.trimmed().toUpper() : QStringLiteral("?");
    const QString right = b.isValid() ? camelotB.trimmed().toUpper() : QStringLiteral("?");
    return QStringLiteral("%1 -> %2").arg(left, right);
}

} // namespace mixxx::music_sync
