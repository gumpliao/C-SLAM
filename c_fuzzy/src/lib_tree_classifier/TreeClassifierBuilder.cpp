/*
 * c_fuzzy,
 *
 *
 * Copyright (C) 2013 Davide Tateo
 * Versione 1.0
 *
 * This file is part of c_fuzzy.
 *
 * c_fuzzy is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * c_fuzzy is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with c_fuzzy.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <fstream>
#include <stdexcept>
#include <sstream>

#include "TreeClassifierBuilder.h"

#include "TreeClassifierParser.tab.h"
#include "TreeClassifierScanner.h"

using namespace std;

TreeClassifierBuilder::TreeClassifierBuilder()
{
	classifier = new FuzzyClassifier();
}

void TreeClassifierBuilder::parse(const char *filename)
{
	ifstream inputFile(filename);
	if (!inputFile.good())
	{
		throw runtime_error("Bad file to parse");
	}

	tc::TreeClassifierScanner* scanner = new tc::TreeClassifierScanner(
				&inputFile);
	tc::TreeClassifierParser* parser = new tc::TreeClassifierParser(*this,
				*scanner);

	if (parser->parse() == -1)
	{
		throw runtime_error("Parse Failed");
	}

	if (parser)
			delete (parser);

		if (scanner)
			delete (scanner);

}

FuzzyClassifier* TreeClassifierBuilder::buildFuzzyClassifier()
{
	checkConsistency();

	classifier->setupClassifier();

	return classifier;
}

VariableList* TreeClassifierBuilder::buildVariableList(VariableList* list,
			string variable)
{

	list = eventuallyInitialize(list);

	auto&& ret = list->insert(variable);

	if (!ret.second)
	{
		stringstream ss;
		ss << "Error: redeclaration of variable " << variable;
		throw runtime_error(ss.str());
	}

	return list;
}

ConstantList* TreeClassifierBuilder::buildCostantList(ConstantList* list,
			string constant, std::string value)
{
	list = eventuallyInitialize(list);

	if (list->count(constant) != 0)
	{
		stringstream ss;
		ss << "Error: redeclaration of constant " << constant;
		throw runtime_error(ss.str());
	}

	ConstantList& listRef = *list;

	listRef[constant] = value;

	return list;
}

FuzzyConstraint* TreeClassifierBuilder::buildSimpleFeature(string variable,
			string fuzzyLabel)
{
	return new FuzzySimpleConstraint(variable, fuzzyLabel);
}

FuzzyConstraint* TreeClassifierBuilder::buildSimpleRelation(
			vector<string>& labelList)
{
	string className = labelList[0];
	string member = labelList[1];
	string matchingVar = labelList[2];
	string fuzzyLabel = labelList[3];

	return new FuzzySimpleRelation(className, member, matchingVar, fuzzyLabel);
}

FuzzyConstraint* TreeClassifierBuilder::buildComplexRelation(
			vector<string>& labelList)
{
	string className = labelList[0];
	string member = labelList[1];
	string variable1 = labelList[2];
	string variable2 = labelList[3];
	string fuzzyLabel = labelList[4];

	return new FuzzyComplexRelation(className, member, variable1, variable2,
				fuzzyLabel);

}

FuzzyConstraint* TreeClassifierBuilder::buildInverseRelation(
			vector<string>& labelList)
{
	string variable = labelList[0];
	string className = labelList[1];
	string member1 = labelList[2];
	string member2 = labelList[3];
	string fuzzyLabel = labelList[4];

	return new FuzzyInverseRelation(className, variable, member1, member2,
				fuzzyLabel);

}

FuzzyConstraintsList* TreeClassifierBuilder::buildFeaturesList(
			FuzzyConstraintsList* list, vector<string>& labelList, FeatureType type)
{
	FuzzyConstraint* feature;

	list = eventuallyInitialize(list);

	switch (type)
	{
		case SIM_C:
			feature = buildSimpleFeature(labelList[0], labelList[1]);
			break;

		case SIM_R:
			feature = buildSimpleRelation(labelList);
			break;

		case COM_R:
			feature = buildComplexRelation(labelList);
			break;

		case INV_R:
			feature = buildInverseRelation(labelList);
			break;

		default:
			return list;
	}

	list->push_back(feature);

	return list;
}

void TreeClassifierBuilder::buildClass(string name, string superClassName,
			VariableList* variables, ConstantList* constants,
			FuzzyConstraintsList* featureList, bool hidden)
{
	variables = eventuallyInitialize(variables);
	constants = eventuallyInitialize(constants);
	featureList = eventuallyInitialize(featureList);
	FuzzyClass* superClass = NULL;

	superClass = classifier->getClass(superClassName);

	checkSuperClass(name, superClassName, superClass);
	FuzzyClass* fuzzyClass = new FuzzyClass(name, superClass, variables,
				constants, featureList, hidden);
	classifier->addClass(fuzzyClass);
}

/* Consistency checks */
void TreeClassifierBuilder::checkSuperClass(const string& name,
			const string& superClassName, FuzzyClass* superClass)
{
	if (superClass == NULL && superClassName.compare("") != 0)
	{
		stringstream ss;
		ss << "Error: class " << name << " extends a non declared class "
					<< superClassName;
		throw runtime_error(ss.str());

	}
}

void TreeClassifierBuilder::checkConsistency()
{
	for (auto& it : *classifier)
	{
		string name = it.first;
		FuzzyClass* fuzzyClass = it.second;
		checkFeatureList(*fuzzyClass);
	}
}

void TreeClassifierBuilder::checkFeatureList(FuzzyClass& fuzzyClass)
{
	FuzzyConstraintsList* featuresPointer = fuzzyClass.getfeatureList();

	if (featuresPointer != NULL)
	{
		FuzzyConstraintsList& features = *featuresPointer;
		for (auto& it : features)
		{
			FuzzyConstraint& feature = *it;
			switch (feature.getConstraintType())
			{
				case SIM_C:
					checkVariable(fuzzyClass, feature.getVariables()[0]);
					break;

				case SIM_R:
					checkVariable(fuzzyClass, feature.getVariables()[0]);
					checkRelation(fuzzyClass, feature);
					break;

				case COM_R:
					checkVariable(fuzzyClass, feature.getVariables()[0]);
					checkVariable(fuzzyClass, feature.getVariables()[1]);
					checkRelation(fuzzyClass, feature);
					break;

				case INV_R:
					checkVariable(fuzzyClass, feature.getRelationVariable());
					checkRelation(fuzzyClass, feature);
					break;
			}
		}
	}
}

void TreeClassifierBuilder::checkVariable(FuzzyClass& fuzzyClass,
			string variable)
{
	if (!fuzzyClass.containsVar(variable))
	{
		stringstream ss;
		ss << "Error: rule in class " << fuzzyClass.getName()
					<< " references non existing variable " << variable;
		throw runtime_error(ss.str());
	}
}

void TreeClassifierBuilder::checkRelationVar(string relatedVariable,
			FuzzyClass& relatedClass, FuzzyClass& fuzzyClass)
{
	if (!relatedClass.containsVar(relatedVariable))
	{
		stringstream ss;
		ss << "Error: relation in class " << fuzzyClass.getName()
					<< " references non existing variable " << relatedVariable;
		throw runtime_error(ss.str());
	}
}

void TreeClassifierBuilder::checkRelation(FuzzyClass& fuzzyClass,
			FuzzyConstraint& relation)
{
	string object = relation.getRelationObject();

	if (!classifier->contains(object))
	{
		stringstream ss;
		ss << "Error: relation in class " << fuzzyClass.getName()
					<< " references non existing class " << object;
		throw runtime_error(ss.str());
	}

	FuzzyClass& relatedClass = *classifier->getClass(object);

	if (relation.getConstraintType() == INV_R)
	{
		vector<string> relatedVariables = relation.getVariables();

		checkRelationVar(relatedVariables[0], relatedClass, fuzzyClass);
		checkRelationVar(relatedVariables[1], relatedClass, fuzzyClass);
	}
	else
	{
		string relatedVariable = relation.getRelationVariable();

		checkRelationVar(relatedVariable, relatedClass, fuzzyClass);
	}

	classifier->addDependency(fuzzyClass.getName(), object);
}
