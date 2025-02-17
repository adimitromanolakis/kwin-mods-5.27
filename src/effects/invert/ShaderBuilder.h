#pragma once

#include <QAction>
#include <QFile>
#include <kwinglutils.h>
#include <kwinglplatform.h>
#include <KLocalizedString>



struct ParametersFloat { QString name; double value; };
struct ParametersInt { QString name; int value; };


class ShaderBuilder {

public:

    ShaderBuilder()
        : m_parameters_int{
            {"method", 4},
            {"SATURATION_ENABLED", 1},
            {"DLS_ENABLED", 1},
            {"CAS_ENABLED", 1},
            {"FILM_NOISE_ENABLED", 1},
        },
        m_parameters{
            { "SATURATION_MIX"  , 0.9 },
            { "DLS_SHARPEN"     , 0.3 },
            { "DLS_DENOISE"     , 0.2 },
            { "CAS_SHARPNESS"   , 0.5 },
            { "FILM_NOISE_STRENGTH", 10 }
        } 
    {}

    QString test();
    void setParameter(QStringList& lines, QString paramName, QString type, double value);

    void disableShader(QStringList& lines, QString shaderName);

    std::vector<ParametersInt> m_parameters_int; // Replacing the array with a vector
    std::vector<ParametersFloat> m_parameters; // Replacing the array with a vector

    void printParams() {

        for( auto k : m_parameters_int) qWarning() << " XX PARAM: " << k.name << " " << k.value;
        for( auto k : m_parameters) qWarning() << " XX PARAM: " << k.name << " " << k.value;

    }

};