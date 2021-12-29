@GUI::Widget {
    name: "main"
    fill_with_background_color: true

    layout: @GUI::VerticalBoxLayout {
        spacing: 2
    }

    @GUI::ToolbarContainer {
        name: "toolbar_container"

        @GUI::Toolbar {
            name: "toolbar"
        }
    }

    @GUI::VerticalSplitter {
        @GUI::HorizontalSplitter {
            @HexEditor::HexEditor {
                name: "editor"
            }

            @GUI::Widget {
                name: "search_results_container"
                visible: false

                layout: @GUI::VerticalBoxLayout {
                }

                @GUI::TableView {
                    name: "search_results"
                }
            }
        }

        @GUI::Widget {
            name: "struct_container"
            visible: false

            layout: @GUI::HorizontalBoxLayout {
            }

            @GUI::TableView {
                name: "structs"
            }

            @GUI::Widget {
                layout: @GUI::VerticalBoxLayout {
                }

                fixed_width: 50

                @GUI::Button {
                    name: "struct_load_button"
                    text: "..."
                }

                @GUI::Button {
                    name: "struct_add_button"
                    text: "+"
                }

                @GUI::Button {
                    name: "struct_remove_button"
                    text: "-"
                }
            }
        }
    }

    @GUI::Statusbar {
        name: "statusbar"
        label_count: 5
    }
}
