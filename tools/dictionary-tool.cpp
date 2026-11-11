// SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
// Native port of the upstream PKD1-to-AOSP dictionary generator.
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QTextStream>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <vector>
int main(int argc,char **argv) {
 QCoreApplication app(argc,argv);
 if(argc!=4)return 2;
 QFile input(QString::fromLocal8Bit(argv[1]));
 if(!input.open(QIODevice::ReadOnly))return 3;
 const QByteArray data=input.readAll();
 if(data.size()<12 || !data.startsWith("PKD1"))return 4;
 const quint32 n=qFromLittleEndian<quint32>(data.constData()+4);
 const quint64 pool=8+quint64(n+1)*4;
 if(n>4000000 || pool>quint64(data.size()))return 4;
 auto offset=[&](quint32 i){return qFromLittleEndian<quint32>(data.constData()+8+quint64(i)*4);};
 const quint64 freqs=pool+offset(n);
 if(freqs+quint64(n)*4>quint64(data.size()))return 4;
 const QString lang=QString::fromLocal8Bit(argv[3]);
 const QString alphabet=lang==QLatin1String("ru")?QStringLiteral("абвгдежзийклмнопрстуфхцчшщъыьэюя"):QStringLiteral("abcdefghijklmnopqrstuvwxyz'");
 QHash<QString,quint32> best; quint64 corpus=0;
 for(quint32 i=0;i<n;i++) {
  const quint32 f=qFromLittleEndian<quint32>(data.constData()+freqs+quint64(i)*4);corpus+=f;
  if(offset(i)>offset(i+1)||offset(i+1)>offset(n))return 4;
  QString word=QString::fromUtf8(data.constData()+pool+offset(i),offset(i+1)-offset(i)).toLower();
  if(lang==QLatin1String("ru"))word.replace(QChar(0x451),QChar(0x435));
  if(word.size()<2||word.size()>24||!f)continue;
  if(std::any_of(word.begin(),word.end(),[&](QChar c){return !alphabet.contains(c);}))continue;
  best[word]=std::max(best.value(word),f);
 }
 std::vector<std::pair<QString,quint32>> words;words.reserve(best.size());
 for(auto i=best.cbegin();i!=best.cend();++i)words.emplace_back(i.key(),i.value());
 std::sort(words.begin(),words.end(),[](const auto&a,const auto&b){return a.second!=b.second?a.second>b.second:a.first<b.first;});
 if(words.size()>50000)words.resize(50000);
 const QString path=QString::fromLocal8Bit(argv[2]);QDir().mkpath(QFileInfo(path).absolutePath());
 QFile output(path);if(!output.open(QIODevice::WriteOnly|QIODevice::Truncate))return 5;
 QTextStream stream(&output);
 stream<<"dictionary=main:"<<lang<<",locale="<<lang<<",description=uke-plasma-keyboard-custom,date=0,version=54\n";
 for(const auto&[word,count]:words){const int f=std::clamp(int(std::nearbyint(27.5*std::log10(double(count)/std::max<quint64>(1,corpus)*1e9)-1)),1,255);stream<<" word="<<word<<",f="<<f<<",flags=,originalFreq="<<count<<'\n';}
 return stream.status()==QTextStream::Ok?0:6;
}
