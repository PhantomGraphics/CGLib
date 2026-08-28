#pragma once
#include <vector>
#include <any>
#include <string>
#include "CGLib/Math/Vector3d.h"

namespace Phantom {
	namespace File {

		/// @brief Data type identifiers used in PLY files.
		enum class PLYType
		{
			CHAR,   ///< Signed 8-bit integer.
			UCHAR,  ///< Unsigned 8-bit integer.
			SHORT,  ///< Signed 16-bit integer.
			USHORT, ///< Unsigned 16-bit integer.
			INT,    ///< Signed 32-bit integer.
			UINT,   ///< Unsigned 32-bit integer.
			FLOAT,  ///< Single-precision floating-point.
			DOUBLE, ///< Double-precision floating-point.
		};

		/// @brief Holds the property values of a single PLY vertex.
		struct PLYPoint
		{
			PLYPoint() = default;

			std::vector<std::any> values; ///< List of property values.

			/// @brief Returns the value at the given index cast to type T.
			/// @tparam T The type to cast to.
			/// @param index Index into the values array.
			/// @return The value cast to T.
			template<typename T>
			T getValueAs(const size_t index) const
			{
				return std::any_cast<T>(values[index]);
			}
		};

		/// @brief Describes a single PLY property (name and type).
		struct PLYProperty
		{
			PLYProperty() = default;

			/// @brief Constructs with the given name and type.
			/// @param name Property name.
			/// @param type Property data type.
			PLYProperty(const std::string& name, const PLYType& type) :
				name(name),
				type(type)
			{}

			/// @brief Returns the type name string.
			/// @return Type name (currently always "float").
			std::string getTypeName() const {
				return "float";
			}

			/// @brief Converts a type name string to a PLYType value.
			/// @param typeName Type name string.
			/// @return Corresponding PLYType.
			static PLYType toType(const std::string& typeName)
			{
				return PLYType::FLOAT;
			}


			std::string name; ///< Property name.
			PLYType type;     ///< Property data type.
		};

		/// @brief Holds the entire contents of a PLY file.
		struct PLYFile
		{
			std::vector<PLYProperty> properties;              ///< Vertex property definitions.
			std::vector<PLYPoint> vertices;                   ///< Vertex data.
			std::vector<std::vector<unsigned int>> faces;     ///< Face index lists.
		};

	}
}
