#include <Godot.hpp>
#include <VisualServer.hpp>
#include <Physics2DServer.hpp>
#include <Viewport.hpp>
#include <Font.hpp>

#include "bullets_pool.h"

using namespace godot;

//-- START Default "standard" implementations.

template <class Kit, class BulletType>
void AbstractBulletsPool<Kit, BulletType>::_init_bullet(BulletType* bullet) {}

template <class Kit, class BulletType>
void AbstractBulletsPool<Kit, BulletType>::_enable_bullet(BulletType* bullet) {
	bullet->lifetime = 0.0f;
	Rect2 texture_rect = Rect2(-kit->texture->get_size() / 2.0f, kit->texture->get_size());
	RID texture_rid = kit->texture->get_rid();
	
	VisualServer::get_singleton()->canvas_item_add_texture_rect(bullet->item_rid,
		texture_rect,
		texture_rid);
}

template <class Kit, class BulletType>
void AbstractBulletsPool<Kit, BulletType>::_disable_bullet(BulletType* bullet) {
	VisualServer::get_singleton()->canvas_item_clear(bullet->item_rid);
}

template <class Kit, class BulletType>
bool AbstractBulletsPool<Kit, BulletType>::_process_bullet(BulletType* bullet, float delta) {
	bullet->transform.set_origin(bullet->transform.get_origin() + bullet->velocity * delta);

	if(!active_rect.has_point(bullet->transform.get_origin())) {
		return true;
	}

	bullet->lifetime += delta;
	return false;
}

//-- END Default "standard" implementation.

template <class Kit, class BulletType>
AbstractBulletsPool<Kit, BulletType>::~AbstractBulletsPool() {
	// Bullets object is responsible for clearing all the area shapes
	for(int32_t i = 0 - 1; i < pool_size; i++) {
		VisualServer::get_singleton()->free_rid(bullets[i]->item_rid);
		bullets[i]->free();
	}
	VisualServer::get_singleton()->free_rid(canvas_item);

	delete[] bullets;
	delete[] shapes_to_indices;
}

template <class Kit, class BulletType>
void AbstractBulletsPool<Kit, BulletType>::_init(Ref<BulletKit> kit, CanvasItem* canvas_parent, int32_t z_index,
		RID shared_area, int32_t starting_shape_index, int32_t pool_size) {
	this->bullet_kit = kit;
	this->kit = kit;
	this->collisions_enabled = kit->collisions_enabled;
	this->canvas_parent = canvas_parent;
	this->shared_area = shared_area;
	this->starting_shape_index = starting_shape_index;
	this->pool_size = pool_size;
	
	available_bullets = pool_size;
	active_bullets = 0;

	bullets = new BulletType*[pool_size];
	shapes_to_indices = new int32_t[pool_size];

	canvas_item = VisualServer::get_singleton()->canvas_item_create();
	VisualServer::get_singleton()->canvas_item_set_parent(canvas_item, canvas_parent->get_canvas_item());
	VisualServer::get_singleton()->canvas_item_set_z_index(canvas_item, z_index);

	RID shared_shape_rid = kit->collision_shape->get_rid();

	for(int32_t i = 0; i < pool_size; i++) {
		BulletType* bullet = BulletType::_new();
		bullets[i] = bullet;

		bullet->item_rid = VisualServer::get_singleton()->canvas_item_create();
		VisualServer::get_singleton()->canvas_item_set_parent(bullet->item_rid, canvas_item);
		VisualServer::get_singleton()->canvas_item_set_material(bullet->item_rid, kit->material->get_rid());

		if(collisions_enabled) {
			Physics2DServer::get_singleton()->area_add_shape(shared_area, shared_shape_rid, Transform2D(), true);
			bullet->shape_index = starting_shape_index + i;
			shapes_to_indices[i] = i;
		}

		Color color = Color(1.0f, 1.0f, 1.0f, 1.0f);
		switch(kit->unique_modulate_component) {
			case 1: // Red
				color.r = fmod(bullet->shape_index * 0.7213f, 1.0f);
				break;
			case 2: // Green
				color.g = fmod(bullet->shape_index * 0.7213f, 1.0f);
				break;
			case 3: // Blue
				color.b = fmod(bullet->shape_index * 0.7213f, 1.0f);
				break;
			case 4: // Alpha
				color.a = fmod(bullet->shape_index * 0.7213f, 1.0f);
				break;
			default: // None or other values
				break;
		}
		VisualServer::get_singleton()->canvas_item_set_modulate(bullet->item_rid, color);

		_init_bullet(bullet);
	}
}

template <class Kit, class BulletType>
int32_t AbstractBulletsPool<Kit, BulletType>::_process(float delta) {
	if(kit->use_viewport_as_active_rect) {
		active_rect = canvas_parent->get_viewport()->get_visible_rect();
	} else {
		active_rect = kit->active_rect;
	}
	int32_t amount_variation = 0;

	if(collisions_enabled) {
		for(int32_t i = pool_size - 1; i >= available_bullets; i--) {
			BulletType* bullet = bullets[i];

			if(_process_bullet(bullet, delta)) {
				_release_bullet(i);
				amount_variation -= 1;
				continue;
			}
			
			VisualServer::get_singleton()->canvas_item_set_transform(bullet->item_rid, bullet->transform);
			Physics2DServer::get_singleton()->area_set_shape_transform(shared_area, bullet->shape_index, bullet->transform);
		}
	} else {
		for(int32_t i = pool_size - 1; i >= available_bullets; i--) {
			BulletType* bullet = bullets[i];

			if(_process_bullet(bullet, delta)) {
				_release_bullet(i);
				amount_variation -= 1;
				continue;
			}
			
			VisualServer::get_singleton()->canvas_item_set_transform(bullet->item_rid, bullet->transform);
		}
	}
	return amount_variation;
}

template <class Kit, class BulletType>
void AbstractBulletsPool<Kit, BulletType>::_draw(Ref<Font> debug_font) {
	for(int32_t i = pool_size - 1; i >= available_bullets; i--) {
		BulletType* bullet = bullets[i];
		canvas_parent->draw_string(debug_font, bullet->transform.get_origin() + Vector2(4, 2), Variant(bullet->shape_index));
		if(collisions_enabled)
			canvas_parent->draw_circle(Physics2DServer::get_singleton()->area_get_shape_transform(shared_area, bullet->shape_index).get_origin(), 0.5f, Color(1, 0, 0, 1));
	}
}

template <class Kit, class BulletType>
void AbstractBulletsPool<Kit, BulletType>::spawn_bullet(Dictionary properties) {
	if(available_bullets > 0) {
		available_bullets -= 1;
		active_bullets += 1;

		BulletType* bullet = bullets[available_bullets];

		if(collisions_enabled)
			Physics2DServer::get_singleton()->area_set_shape_disabled(shared_area, bullet->shape_index, false);

		Array keys = properties.keys();
		for(int32_t i = 0; i < keys.size(); i++) {
			bullet->set(keys[i], properties[keys[i]]);
		}

		_enable_bullet(bullet);
	}
}

template <class Kit, class BulletType>
BulletID AbstractBulletsPool<Kit, BulletType>::obtain_bullet() {
	if(available_bullets > 0) {
		available_bullets -= 1;
		active_bullets += 1;

		BulletType* bullet = bullets[available_bullets];

		if(collisions_enabled)
			Physics2DServer::get_singleton()->area_set_shape_disabled(shared_area, bullet->shape_index, false);

		_enable_bullet(bullet);

		return BulletID(bullet->shape_index, bullet->cycle);
	}
	return BulletID(-1, -1);
}

template <class Kit, class BulletType>
bool AbstractBulletsPool<Kit, BulletType>::release_bullet(BulletID id) {
	if(id.index >= starting_shape_index && id.index < starting_shape_index + pool_size) {
		int32_t bullet_index = shapes_to_indices[id.index - starting_shape_index];
		if(bullet_index >= available_bullets && bullet_index < pool_size && id.cycle == bullets[bullet_index]->cycle) {
			_release_bullet(bullet_index);
			return true;
		}
	}
	return false;
}

template <class Kit, class BulletType>
void AbstractBulletsPool<Kit, BulletType>::_release_bullet(int32_t index) {
	BulletType* bullet = bullets[index];
	
	// Implement bullet recycling, defer the disabling, add the bullet to a list of bullets to disable,
	// (can also be the available section of the list)
	// increment a counter, at the beginning of the frame remove the shapes, if a bullet is obtained in the meantime,
	// use one of the ones scheduled for disabling.
	if(collisions_enabled)
		Physics2DServer::get_singleton()->area_set_shape_disabled(shared_area, bullet->shape_index, true);
	
	_disable_bullet(bullet);
	bullet->cycle += 1;

	_swap(shapes_to_indices[bullet->shape_index - starting_shape_index], shapes_to_indices[bullets[available_bullets]->shape_index - starting_shape_index]);
	_swap(bullets[index], bullets[available_bullets]);

	available_bullets += 1;
	active_bullets -= 1;
}

template <class Kit, class BulletType>
bool AbstractBulletsPool<Kit, BulletType>::is_bullet_valid(BulletID id) {
	if(id.index >= starting_shape_index && id.index < starting_shape_index + pool_size) {
		int32_t bullet_index = shapes_to_indices[id.index - starting_shape_index];
		if(bullet_index >= available_bullets && bullet_index < pool_size && id.cycle == bullets[bullet_index]->cycle) {
			return true;
		}
	}
	return false;
}

template <class Kit, class BulletType>
BulletID AbstractBulletsPool<Kit, BulletType>::get_bullet_from_shape(int32_t shape_index) {
	if(shape_index >= starting_shape_index && shape_index < starting_shape_index + pool_size) {
		int32_t bullet_index = shapes_to_indices[shape_index - starting_shape_index];
		if(bullet_index >= available_bullets) {
			return BulletID(shape_index, bullets[bullet_index]->cycle);
		}
	}
	return BulletID(-1, -1);
}


template <class Kit, class BulletType>
void AbstractBulletsPool<Kit, BulletType>::set_bullet_property(BulletID id, String property, Variant value) {
	if(is_bullet_valid(id)) {
		int32_t bullet_index = shapes_to_indices[id.index - starting_shape_index];
		bullets[bullet_index]->set(property, value);
	}
}

template <class Kit, class BulletType>
Variant AbstractBulletsPool<Kit, BulletType>::get_bullet_property(BulletID id, String property) {
	if(is_bullet_valid(id)) {
		int32_t bullet_index = shapes_to_indices[id.index - starting_shape_index];

		return bullets[bullet_index]->get(property);
	}
	return Variant();
}