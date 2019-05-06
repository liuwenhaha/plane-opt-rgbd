#include "partition.h"
#include <stdlib.h>

// Good reference for set:
// https://stackoverflow.com/questions/7340434/how-to-update-an-existing-element-of-stdset
// https://stackoverflow.com/questions/18295333/how-to-efficiently-insert-a-range-of-consecutive-integers-into-a-stdset


bool Partition::readPLY(const std::string& filename)
{
	FILE* fin;
	if (!(fin = fopen(filename.c_str(), "rb")))
	{
		cout << "ERROR: Unable to open file" << filename << endl;
		return false;
	}

	/************************************************************************/
	/* Read headers */

	// Mode for vertex and face type
	// 1 for vertex (no faces), 2 for vertex and faces,
	// 3 for vertex, vertex colors (no faces), 4 for vertex, vertex colors and faces
	int vertex_mode = 1;
	int ply_mode = 0;	  // 0 for ascii, 1 for little-endian
	int color_channel_num = 0;
	int vertex_quality_dim = 0; // vertex quality, such as vertex normal or other data per vertex defined by user
	int vertex_normal_dim = 0; // vertex normal dimension
	char seps[] = " ,\t\n\r "; // separators
	seps[5] = 10;
	int property_num = 0;
	char line[1024];
    maxcoord_[0] = maxcoord_[1] = maxcoord_[2] = std::numeric_limits<double>::min();
    mincoord_[0] = mincoord_[1] = mincoord_[2] = std::numeric_limits<double>::max();
    center_[0] = center_[1] = center_[2] = 0;

	while (true)
	{
		fgets(line, 1024, fin);
		char* token = strtok(line, seps);
		if (!strcmp(token, "end_header")) break;
		else if (!strcmp(token, "format"))
		{
			token = strtok(NULL, seps);
			if (!strcmp(token, "ascii"))
				ply_mode = 0;
			else if (!strcmp(token, "binary_little_endian"))
				ply_mode = 1;
			else
			{
				cout << "WARNING: can not read this type of PLY model: " << std::string(token) << endl;
				return false;
			}
		}
		else if (!strcmp(token, "element"))
		{
			token = strtok(NULL, seps);
			if (!strcmp(token, "vertex"))
			{
				// vertex count
				token = strtok(NULL, seps);
				sscanf(token, "%d", &vertex_num_);
				vertex_mode = 1;
			}
			else if (!strcmp(token, "face"))
			{
				// Face count
				token = strtok(NULL, seps);
				sscanf(token, "%d", &face_num_);
				vertex_mode++; // mode with faces is 1 larger than mode without faces
			}
		}
		else if (!strcmp(token, "property"))
		{
			if (vertex_mode % 2 == 1)
			{
				if (property_num >= 3) // skip property x,y,z
				{
					token = strtok(NULL, seps);
					if (!strcmp(token, "uchar")) // color
					{
						token = strtok(NULL, seps);
						color_channel_num++;
						if (color_channel_num >= 3) // color channel number must be 3 (RGB) or 4 (RGBA)
							vertex_mode = 3;
					}
					else if (!strcmp(token, "float")) // vertex quality data
					{
						// Currently just count it and skip
						token = strtok(NULL, seps);
						if (!strcmp(token, "nx") || !strcmp(token, "ny") || !strcmp(token, "nz"))
							vertex_normal_dim++;
						else
							vertex_quality_dim++;
					}
				}
				property_num++;
			}
			else if (vertex_mode % 2 == 0)
			{
				token = strtok(NULL, seps);
				bool face_flag = false;
				if (!strcmp(token, "list"))
				{
					token = strtok(NULL, seps);
					if (!strcmp(token, "uint8") || !strcmp(token, "uchar"))
					{
						token = strtok(NULL, seps);
						if (!strcmp(token, "int") || !strcmp(token, "int32"))
							face_flag = true;
					}
					if (!face_flag)
					{
						cout << "ERROR in Reading PLY model: the type of 'number of face indices' is not 'unsigned char', or the type of 'vertex_index' is not 'int'." << endl;
						return false;
					}
				}
			}
		}
	}
	if (color_channel_num != 0 && color_channel_num != 3 && color_channel_num != 4)
	{
		cout << "ERROR: Color channel number is " << color_channel_num << " but it has to be 0, 3, or 4." << endl;
		return false;
	}
	if (vertex_normal_dim != 0 && vertex_normal_dim != 3)
	{
		cout << "ERROR: Vertex normal dimension is " << vertex_normal_dim << " but it has to be 0 or 3." << endl;
		return false;
	}

	/************************************************************************/
	/* Read vertices and faces */
	vertices_.reserve(vertex_num_);
	faces_.reserve(face_num_);
	if (ply_mode == 1) // binary mode
	{
		for (int i = 0; i < vertex_num_; i++)
		{
			// The vertex data order is: coordinates -> normal -> color -> others (qualities, radius, curvatures, etc)
			size_t haveread = 0;
			Vertex vtx;
			float vert[3];
			if ((haveread = fread(vert, sizeof(float), 3, fin)) != 3)
			{
				cout << "ERROR in reading PLY vertices in position " << ftell(fin) << endl;
				return false;
			}
			if (vertex_normal_dim)
			{
				float nor[3];
				if ((haveread = fread(nor, sizeof(float), vertex_normal_dim, fin)) != vertex_normal_dim)
				{
					cout << "ERROR in reading PLY vertex normals in position " << ftell(fin) << endl;
					return false;
				}
				// Currently we just abandon the vertex normal
				//vtx.normal = Vector3d(nor[0], nor[1], nor[2]);
			}
			if (color_channel_num)
			{
				unsigned char color[4];
				if ((haveread = fread(color, sizeof(unsigned char), color_channel_num, fin)) != color_channel_num)
				{
					cout << "ERROR in reading PLY vertex colors in position " << ftell(fin) << endl;
					return false;
				}
				vtx.color = Vector3f(color[0], color[1], color[2]) / 255;
			}
			if (vertex_quality_dim)
			{
				float qual[3]; // Currently we just abandon the vertex quality data
				if ((haveread = fread(qual, sizeof(float), vertex_quality_dim, fin)) != vertex_quality_dim)
				{
					cout << "ERROR in reading PLY vertex qualities in position " << ftell(fin) << endl;
					return false;
				}
				// Currently we just abandon the vertex normal
			}
			vtx.pt = Vector3d(vert[0], vert[1], vert[2]);
			vertices_.push_back(vtx);
			for (int j = 0; j < 3; ++j)
			{
				mincoord_[j] = min(mincoord_[j], double(vert[j]));
				maxcoord_[j] = max(maxcoord_[j], double(vert[j]));
				center_[j] += vert[j];
			}
		}

		// Face data
		for (int i = 0; i < face_num_; ++i)
		{
			unsigned char channel_num;
			size_t haveread = fread(&channel_num, 1, 1, fin); // channel number for each face
			Face fa;
			if ((haveread = fread(fa.indices, sizeof(int), 3, fin)) != 3) // currently only support triangular face
			{
				cout << "ERROR in reading PLY face indices in position " << ftell(fin) << endl;
				return false;
			}
			faces_.push_back(fa);
		}
	}
	else // ASCII mode (face reader is still unfinished)
	{
		// Read vertices
		for (int i = 0; i < vertex_num_; i++)
		{
			// The vertex data order is: coordinates -> normal -> color -> others (qualities, radius, curvatures, etc)
			fgets(line, 1024, fin);
			char* token = strtok(line, seps);
			// Read 3D point
			Vertex vtx;
			float vert[3];
			for (int j = 0; j < 3; ++j)
			{
				token = strtok(NULL, seps);
				sscanf(token, "%f", &(vert[j]));
			}
			// Read vertex normal
			if (vertex_normal_dim)
			{
				float nor[3];
				for (int j = 0; j < vertex_normal_dim; ++j)
				{
					token = strtok(NULL, seps);
					sscanf(token, "%f", &(nor[j]));
				}
				// Currently we just abandon the vertex normal data
				//vtx.normal = Vector3d(nor[0], nor[1], nor[2]);
			}
			if (color_channel_num)
			{
				unsigned char color[4];
				for (int j = 0; j < vertex_quality_dim; ++j)
				{
					token = strtok(NULL, seps);
					sscanf(token, "%c", &(color[j]));
				}
				vtx.color = Vector3f(color[0], color[1], color[2]) / 255;
			}
			if (vertex_quality_dim)
			{
				float qual;
				for (int j = 0; j < vertex_quality_dim; ++j)
				{
					token = strtok(NULL, seps);
					sscanf(token, "%f", &qual);
				}
				// Currently we just abandon the vertex quality data
			}
			vtx.pt = Vector3d(vert[0], vert[1], vert[2]);
			vertices_.push_back(vtx);
			for (size_t j = 0; j < 3; ++j)
			{
				mincoord_[j] = min(mincoord_[j], double(vert[j]));
				maxcoord_[j] = max(maxcoord_[j], double(vert[j]));
				center_[j] += vert[j];
			}
		}
		// Read Faces
		for (int i = 0; i < face_num_; i++)
		{
			fgets(line, 1024, fin);
			char* token = strtok(line, seps);
			token = strtok(NULL, seps);
			for (int j = 0; j < 3; ++j)
			{
				token = strtok(NULL, seps);
				sscanf(token, "%d", &(faces_[i].indices[j]));
			}
		}
	}
	/* Note to compute center after reading all data */
	for (int j = 0; j < 3; ++j)
	{
		center_[j] /= vertex_num_;
	}
	return true;
}