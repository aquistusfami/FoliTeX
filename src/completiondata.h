#pragma once
#include <QHash>
#include <QStringList>

struct PackageWords
{
    QStringList commands;
    QStringList environments;
};

const QHash<QString, PackageWords> &curatedPackages();
