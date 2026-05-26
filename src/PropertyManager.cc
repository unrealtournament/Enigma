/*
 * Copyright (C) 2006 Ronald Lamprecht
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "PropertyManager.hh"
#include "errors.hh"
#include "main.hh"
#include "Utf8ToXML.hh"
#include "XMLtoUtf8.hh"
#include "ecl_system.hh"
#include <cstdio>
#include <xercesc/dom/DOM.hpp>
#include <xercesc/util/XMLDouble.hpp>
#include <xercesc/util/XMLString.hpp>
#include <xercesc/util/XercesVersion.hpp>

using xercesc::DOMElement;
using xercesc::DOMNodeList;
using xercesc::XMLDouble;
using xercesc::XMLString;

namespace enigma {
    PropertyManager::PropertyManager() : doc (nullptr), propertiesElem (nullptr) {
    }
    
    PropertyManager::~PropertyManager() {
    }
    
    void PropertyManager::setProperty(const char *prefName, const std::string &value) {
        DOMElement * property = getPropertyElement(prefName);
        
        property->setAttribute(Utf8ToXML("value").x_str(),
                Utf8ToXML(&value).x_str());
    }
    
    void PropertyManager::setProperty(const char *prefName, const char *value) {
        setProperty(prefName, std::string(value));
    }
    
   void PropertyManager::getProperty(const char *prefName, std::string &value) {
        DOMElement * property;
       if (hasProperty(prefName, &property)) {
            value = XMLtoUtf8(property->getAttribute(Utf8ToXML("value").x_str())).c_str();
        }
    }

    std::string PropertyManager::getString(const char *prefName) {
        std::string value;
        getProperty(prefName, value);
        return value;
    }
    
    void PropertyManager::setProperty(const char *prefName, const double &value) {
        char printedValue[20];
        sprintf(printedValue, "%.7g", value);
        DOMElement * property = getPropertyElement(prefName);
        property->setAttribute(Utf8ToXML("value").x_str(), 
                Utf8ToXML(printedValue).x_str());
    }
    
    void PropertyManager::getProperty(const char *prefName, double &value) {
        DOMElement * property;
        if (hasProperty(prefName, &property)) {
            XMLDouble * result = new XMLDouble(property->getAttribute(Utf8ToXML("value").x_str()));
            value = result->getValue();
            delete result;
        }
    }

    double PropertyManager::getDouble(const char *prefName) {
        double value = 0;
        getProperty (prefName, value);
        return value;
    }
    
    void PropertyManager::setProperty(const char *prefName, const int &value) {
        char printedValue[20];
        sprintf(printedValue, "%d", value);
        DOMElement * property = getPropertyElement(prefName);

        property->setAttribute(Utf8ToXML("value").x_str(), 
                Utf8ToXML(printedValue).x_str());
    }
    
    void PropertyManager::getProperty(const char *prefName, int &value) {
        DOMElement * property;
        if (hasProperty(prefName, &property)) {
            value = XMLString::parseInt(property->getAttribute(Utf8ToXML("value").x_str()));
        }
    }

    int PropertyManager::getInt(const char *prefName) {
        int value = 0;
        getProperty (prefName, value);
        return value;
    }
    
    void PropertyManager::setProperty(const char *prefName, const bool &value) {
        int i = (value ? 1: 0);
        setProperty(prefName, i);
    }
    
    void PropertyManager::getProperty(const char *prefName, bool &value) {
        int result = 0;
        getProperty(prefName, result);
        value = result != 0;
    }

    bool PropertyManager::getBool(const char *prefName) {
        bool value = false;
        getProperty (prefName, value);
        return value;
    }
    
    DOMElement * PropertyManager::getPropertyElement(const char *prefName) {
        ASSERT(propertiesElem != nullptr, XFrontend, "");
        ASSERT(doc != nullptr, XFrontend, "");
        XMLCh * key = XMLString::replicate(Utf8ToXML(prefName).x_str());
        DOMElement * property;
        bool propFound = hasProperty(prefName, &property);

        if (!propFound) {
            DOMElement * newProperty = doc->createElement (Utf8ToXML("property").x_str());
            newProperty->setAttribute(Utf8ToXML("key").x_str(), key);
            // insert it at the end of the existing properties
            propertiesElem->appendChild(newProperty);
            property = newProperty;
         }
        XMLString::release(&key);
        return property;
    }
    
    bool PropertyManager::hasProperty(const char *prefName, DOMElement ** element) {
        return hasProperty(Utf8ToXML(prefName).x_str(), element);
    }
        
    bool PropertyManager::hasProperty(const XMLCh * key, DOMElement ** element) const {
        ASSERT(propertiesElem != nullptr, XFrontend, "");
        bool propFound = false;
        DOMElement * property = nullptr;

// Xerces 3.0 has no full XPath support - otherwise the following simple
// statement would suffice and not be aborted with NOT_SUPPORTED_ERR.
//        doc->evaluate(Utf8ToXML("//property[@key='...']").x_str(), doc, nullptr, 0, nullptr);

        DOMNodeList * propList = propertiesElem->getElementsByTagName(Utf8ToXML("property").x_str());
        for (int i = 0, l = propList-> getLength(); i < l && !propFound; i++) {
            property = dynamic_cast<DOMElement *>(propList->item(i));
            if (XMLString::equals(key, 
                    property->getAttribute(Utf8ToXML("key").x_str()))) {
                propFound = true;
            }
        }
        *element = property;
        return propFound;
    }
} // namespace enigma
